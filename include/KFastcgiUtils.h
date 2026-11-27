/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */

#ifndef KFASTCGIUTILS_H_
#define KFASTCGIUTILS_H_
#include <string.h>
#include <limits.h>
#include "global.h"
#include "KBuffer.h"
#include "KEnvInterface.h"
#include "fastcgi.h"
#include "log.h"
void initFastcgiData();
extern FCGI_BeginRequestRecord fastRequestStart, fastRequestStartKeepAlive;
class KFastcgiParser
{
public:
	bool parse_param(KEnvInterface* env, u_char* pos, u_char* end)
	{
		if (env == NULL || pos == NULL || end == NULL || pos > end) {
			return false;
		}
		while (pos < end) {
			unsigned name_len, value_len;
			pos = parse_len(pos, end, &name_len);
			if (pos == NULL) {
				return false;
			}
			pos = parse_len(pos, end, &value_len);
			if (pos == NULL) {
				return false;
			}
			size_t remaining = (size_t)(end - pos);
			if (name_len > remaining || value_len > remaining - name_len) {
				return false;
			}
			char* name = (char*)pos;
			pos += name_len;
			char* value = (char*)pos;
			pos += value_len;
			if (!env->add_env(name, name_len, value, value_len)) {
				return false;
			}
		}
		return true;
	}
private:
	u_char* parse_len(u_char* pos, u_char* end, unsigned* len)
	{
		if (end - pos < 1) {
			return NULL;
		}
		if (KBIT_TEST(*pos, 0x80)) {
			if (end - pos < 4) {
				return NULL;
			}
			*len = ((unsigned)(pos[0] & 0x7f) << 24) |
				((unsigned)pos[1] << 16) |
				((unsigned)pos[2] << 8) |
				(unsigned)pos[3];
			return pos + 4;
		}
		*len = *pos;
		return pos + 1;
	}
};
template<typename T>
class KFastcgiStream : public KEnvInterface
{
public:
	KFastcgiStream(T* client, uint8_t data_type, bool extend = false)
	{
		//gcc 4.8不会应用匿名union成员的默认初始化,必须显式清零
		flags = 0;
		setStream(client);
		this->extend = extend;
		readBuf = NULL;
		readHot = NULL;
		readLeft = 0;
		this->data_type = data_type;
	}
	KFastcgiStream(uint8_t data_type)
	{
		flags = 0;
		client = NULL;
		extend = false;
		readBuf = NULL;
		readHot = NULL;
		readLeft = 0;
		this->data_type = data_type;
	}
	bool beginRequest(bool keepAlive = false)
	{
		if (extend) {
			//如果是api内部使用,则不用发送beginRequest,因为已经发送了。
			return true;
		}
		return client->write_all((char*)(keepAlive ? &fastRequestStartKeepAlive : &fastRequestStart),
			sizeof(FCGI_BeginRequestRecord)) == STREAM_WRITE_SUCCESS;
	}
	inline void setStream(T* client)
	{
		this->client = client;
	}
	virtual ~KFastcgiStream()
	{
		if (readBuf) {
			xfree(readBuf);
		}
	}
	bool add_env(const char* attr, size_t attr_len, const char* val, size_t val_len) override
	{
		if (attr == NULL || val == NULL || attr_len == 0 ||
			attr_len > 0xffffU || val_len > 0xffffU) {
			return false;
		}
		size_t encoded_len = attr_len + val_len +
			(attr_len > 127 ? 4 : 1) + (val_len > 127 ? 4 : 1);
		if (encoded_len > 0xffffU) {
			return false;
		}
		if (buff.getLen() > 0xffffU - encoded_len && !sendParams()) {
			return false;
		}
		addLen((unsigned)attr_len);
		addLen((unsigned)val_len);
		//printf("add env name=[%s] value=[%s]\n", attr, val);
		buff.write_all(attr, (int)attr_len);
		buff.write_all(val, (int)val_len);
		return true;
	}
	bool addEnv(const char* name, const char* value) override
	{
		if (name == NULL || value == NULL) {
			return false;
		}
		return add_env(name, strlen(name), value, strlen(value));
	}
	bool write_data(unsigned char type, const char* buf, int len)
	{
		if (len < 0 || (len > 0 && buf == NULL)) {
			return false;
		}
		if (len == 0) {
			return sendRecordHeader(type, 0);
		}
		//printf("write data len=%d\n",len);
		while (len > 0) {
			int this_send = KGL_MIN(len, 32767);
			if (!sendRecordHeader(type, this_send)) {
				return false;
			}
			if (0 != client->write_all(buf, this_send)) {
				return false;
			}
			len -= this_send;
			buf += this_send;
		}
		return true;
	}
	bool write_data(const char* buf, int len)
	{
		return write_data(data_type, buf, len);
	}
	bool write_end(bool close = false)
	{
		//debug("call write_end close=%d\n",close);
		if (!write_data(NULL, 0)) {
			//debug("send STDIN NULL faled\n");
			return false;
		}
		if (close) {
			return sendRecordHeader(FCGI_ABORT_REQUEST, 0);
		}
		//printf("send FCGI_END_REQUEST\n");
		return sendRecordHeader(FCGI_END_REQUEST, 0);
	}
	bool read_package(FCGI_Header* header, char** buffer, int& len)
	{
		if (header == NULL || buffer == NULL) {
			return false;
		}
		*buffer = NULL;
		if (!client->read_all((char*)header, sizeof(*header))) {
			return false;
		}
		len = ntohs(header->contentLength);
		if (len > 0) {
			*buffer = (char*)malloc(len + 1);
			if (*buffer == NULL) {
				return false;
			}
			if (!client->read_all(*buffer, len)) {
				goto failed;
			}
			(*buffer)[len] = '\0';
		} else if (header->type == FCGI_STDOUT || header->type == FCGI_STDIN) {
			data_empty_length = 1;
		}
		if (recvPaddingData(header)) {
			return true;
		}
	failed:
		if (*buffer) {
			xfree(*buffer);
			*buffer = NULL;
		}
		return false;
	}
	bool read(char** buffer, int& len)
	{
		if (data_empty_length) {
			len = 0;
			return true;
		}
		char* b = NULL;
		*buffer = NULL;
		FCGI_Header header;
	read_header:
		if (!client->read_all((char*)&header, sizeof(header))) {
			return false;
		}
		len = ntohs(header.contentLength);
		if (header.type == FCGI_STDOUT || header.type == FCGI_STDIN) {
			assert(b == NULL);
			if (len == 0) {
				data_empty_length = 1;
				return recvPaddingData(&header);
			}
			b = (char*)xmalloc(len);
			if (b == NULL) {
				return false;
			}
			if (client->read_all(b, len) && recvPaddingData(&header)) {
				*buffer = b;
				return true;
			}
			xfree(b);
			return false;
		}
		//	unsigned contentLength = ntohs(header.contentLength);
		if (len > 0) {
			char* buf = (char*)malloc(len + 1);
			if (!client->read_all(buf, len)) {
				xfree(buf);
				return false;
			}
			if (header.type == FCGI_STDERR) {
				buf[len] = 0;
				fprintf(stderr, "[fastcgi] %s\n", buf);
			}
			xfree(buf);
		}
		if (!recvPaddingData(&header)) {
			return false;
		}
		if (header.type == FCGI_END_REQUEST) {
			return true;
		}
		if (header.type == FCGI_ABORT_REQUEST) {
			return false;
		}
		goto read_header;
	}
	/* keep a received data package (malloced), it will be returned by read() later. */
	void push_read_data(char* buf, int len)
	{
		if (len <= 0) {
			xfree(buf);
			return;
		}
		if (readBuf == NULL || readLeft <= 0) {
			if (readBuf) {
				xfree(readBuf);
			}
			readBuf = buf;
			readHot = buf;
			readLeft = len;
			return;
		}
		char* nb = (char*)xmalloc(readLeft + len);
		kgl_memcpy(nb, readHot, readLeft);
		kgl_memcpy(nb + readLeft, buf, len);
		xfree(readBuf);
		xfree(buf);
		readBuf = nb;
		readHot = nb;
		readLeft += len;
	}
	int read(char* buf, int len)
	{
		if (len < 0 || (len > 0 && buf == NULL)) {
			return -1;
		}
		if (len == 0) {
			return 0;
		}
		int readLen = len;
		for (int i = 0; i < 2; i++) {
			if (readBuf && readLeft > 0) {
				if (readLen > readLeft) {
					readLen = readLeft;
				}
				kgl_memcpy(buf, readHot, readLen);
				readHot += readLen;
				readLeft -= readLen;
				return readLen;
			}
			if (readBuf) {
				xfree(readBuf);
				readBuf = NULL;
			}
			if (!read(&readBuf, readLeft)) {
				return -1;
			}
			if (readLeft <= 0) {
				return 0;
			}
			readHot = readBuf;
		}
		return -1;
	}
	bool readParams(KEnvInterface* env)
	{
		if (env == NULL) {
			return false;
		}
		FCGI_Header header;
		for (;;) {
			if (!client->read_all((char*)&header, sizeof(header))) {
				debug("cann't read params header\n");
				return false;
			}
			if (header.version != FCGI_VERSION_1 || header.requestIdB1 != 0 ||
				header.requestIdB0 != 1 || header.type != FCGI_PARAMS) {
				debug("header type =%d is error\n", header.type);
				return false;
			}
			int content_len = ntohs(header.contentLength);
			if (content_len <= 0) {
				if (!recvPaddingData(&header)) {
					return false;
				}
				return env->addEnvEnd();
			}
			if (!readParamsPackage(env, content_len)) {
				debug("cann't read params package\n");
				return false;
			}
			if (!recvPaddingData(&header)) {
				return false;
			}
		}
		/*
		 * 永远不会到达这里
		 */
		return false;
	}
	bool readParamsPackage(KEnvInterface* env, int content_len)
	{
		bool result;
		for (;;) {
			if (content_len <= 0) {
				//printf("read params package success\n");
				return true;
			}
			result = false;

			unsigned name_len = readLen(content_len);

			if (name_len == 0 || name_len == UINT_MAX) {
				debug("name_len is zero\n");
				return false;
			}
			if (name_len > (unsigned)content_len) {
				return false;
			}
			content_len -= name_len;
			unsigned value_len = readLen(content_len);
			if (value_len == UINT_MAX || value_len > (unsigned)content_len) {
				return false;
			}
			/*
			if (value_len == 0) {
				debug("value_len is zero,name_len=%d\n",name_len);
				return false;
			}
			*/
			content_len -= value_len;
			char* name = (char*)xmalloc(name_len + 1);
			char* value = (char*)xmalloc(value_len + 1);
			if (!client->read_all(name, name_len)) {
				debug("cann't read name\n");
				goto error;
			}
			if (value_len > 0 && !client->read_all(value, value_len)) {
				debug("cann't read value\n");
				goto error;
			}
			result = true;
			name[name_len] = '\0';
			value[value_len] = '\0';
			//fprintf(stderr, "success read name=[%s] value=[%s]\n",name,value);
			if (name_len > 5 && strncmp(name, _KS("HTTP_")) == 0) {
				result = env->add_http_header(name + 5, name_len - 5, value, value_len);
			} else {
				result = env->add_env(name, name_len, value,value_len);
			}
		error: xfree(name);
			xfree(value);
			if (!result) {
				return false;
			}
		}
	}
	bool sendParams()
	{
		if (buff.getLen() > 0) {
			if (!sendRecordHeader(FCGI_PARAMS, buff.getLen())) {
				return false;
			}
			kbuf* buf = buff.head;
			while (buf) {
				if (buf->used > 0) {
					if (KGL_OK != client->write_all(buf->data, buf->used)) {
						return false;
					}
				}
				buf = buf->next;
			}
			buff.clean();
		}
		return true;
	}
	bool recvPaddingData(FCGI_Header* header)
	{
		if (header->paddingLength == 0) {
			return true;
		}
		char paddingbuf[256];
		if (!client->read_all(paddingbuf, header->paddingLength)) {
			return false;
		}
		return true;
	}
	bool addEnvEnd() override
	{
		if (!sendParams()) {
			return false;
		}
		if (!sendRecordHeader(FCGI_PARAMS, 0)) {
			return false;
		}
		return true;
	}
	bool sendRecordHeader(unsigned char type, unsigned short contentLength)
	{
		FCGI_Header header;
		memset(&header, 0, sizeof(header));
		header.version = 1;
		header.type = type;
		header.contentLength = htons(contentLength);
		header.requestIdB0 = 1;
		return client->write_all((char*)&header, sizeof(header)) == KGL_OK;
	}

	bool extend;
	bool add_http_header(const char* attr, size_t attr_len, const char* val, size_t val_len) override
	{
		if (!extend) {
			return KEnvInterface::add_http_header(attr, attr_len, val, val_len);
		}
		if (attr == NULL || val == NULL || attr_len > SIZE_MAX - 6) {
			return false;
		}
		char* dst = (char*)xmalloc(attr_len + 6);
		char* hot = dst;
		kgl_memcpy(hot, "HTTP_", 5);
		hot += 5;
		kgl_memcpy(hot, attr, attr_len);
		hot[attr_len] = '\0';
		bool result = add_env(dst, attr_len+5, val, val_len);
		xfree(dst);
		return result;
	}
	bool has_data_empty_arrived()
	{
		return data_empty_length;
	}
	friend class KFastcgiFetchObject;

private:
	union
	{
		struct
		{
			uint8_t data_type;
			uint8_t data_empty_length : 1;
		};
		uint16_t flags = 0;
	};
	void addLen(unsigned len)
	{
		//printf("addLen len=%d\n",len);
		if (len > 127) {
			len = htonl(len | 0x80000000);
			//printf(">127 %x\n",len);
			buff.write_all((char*)&len, 4);
		} else {
			unsigned char char_len = len;
			buff.write_all((char*)&char_len, 1);
		}
	}
	unsigned readLen(int& content_len)
	{
		unsigned char len[4];
		if (content_len < 1) {
			return UINT_MAX;
		}
		if (!client->read_all((char*)len, 1)) {
			return UINT_MAX;
		}
		content_len--;
		if (KBIT_TEST(len[0], 0x80)) {
			if (content_len < 3) {
				return UINT_MAX;
			}
			if (!client->read_all((char*)len + 1, 3)) {
				return UINT_MAX;
			}
			content_len -= 3;
			return ((unsigned)(len[0] & 0x7f) << 24) |
				((unsigned)len[1] << 16) |
				((unsigned)len[2] << 8) |
				(unsigned)len[3];
		}
		return len[0];
	}

	int readLeft;
	char* readBuf;
	char* readHot;
	KBuffer buff;
	T* client;
};
#endif /* KFASTCGIUTILS_H_ */
