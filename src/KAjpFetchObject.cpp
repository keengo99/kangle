/*
 * KAjpFetchObject.cpp
 *
 *  Created on: 2010-7-31
 *      Author: keengo
 */
#include "http.h"
#include "kfeature.h"
#include "KAjpFetchObject.h"
 //#include "KHttpTransfer.h"
#include "KHttpResponseParser.h"
#include "kmalloc.h"
#include "KPoolableSocketContainer.h"
#define MAX_AJP_RESPONSE_HEADERS 0xc
#define MAX_AJP_REQUEST_HEADERS  0xf
#if 0 
static const char* ajp_request_headers[MAX_AJP_REQUEST_HEADERS] = {
	"",
	"accept",
	"accept-charset",
	"accept-encoding",
	"accept-language",
	"authorization",
	"connection",
	"content-type",
	"content-length",
	"cookie",
	"cookie2",
	"host",
	"pragma",
	"referer",
	"user-agent"
};
#endif
static kgl_str_t ajp_response_headers[MAX_AJP_RESPONSE_HEADERS] = {
	kgl_string("Unknow"),
	kgl_string("Content-Type"),
	kgl_string("Content-Language"),
	kgl_string("Content-Length"),
	kgl_string("Date"),
	kgl_string("Last-Modified"),
	kgl_string("Location"),
	kgl_string("Set-Cookie"),
	kgl_string("Set-Cookie2"),
	kgl_string("Servlet-Engine"),
	kgl_string("Status"),
	kgl_string("WWW-Authenticate")
};
KAjpFetchObject::KAjpFetchObject() {
	body_len = 0;
	end_request_recved = false;
}

KAjpFetchObject::~KAjpFetchObject() {
}
//创建发送头到buffer中。
KGL_RESULT KAjpFetchObject::buildHead(KHttpRequest* rq)
{
	assert(buffer == NULL);
	buffer = new KSocketBuffer(AJP_BUFF_SIZE);
	char tmpbuff[50];
	KHttpObject* obj = rq->ctx->obj;
	KBIT_SET(obj->index.flags, ANSW_LOCAL_SERVER);
	KAjpMessage b(buffer);
	b.putByte(JK_AJP13_FORWARD_REQUEST);
	b.putByte(rq->sink->data.meth);
	b.putString("HTTP/1.1");
	if (KBIT_TEST(rq->sink->data.url->flags, KGL_URL_ENCODE)) {
		size_t path_len = 0;
		char* path = url_encode(rq->sink->data.url->path, strlen(rq->sink->data.url->path), &path_len);
		b.putString(path);
		free(path);
	} else {
		b.putString(rq->sink->data.url->path);
	}
	b.putString(rq->getClientIp());
	b.putString("");
	//remote host
	//b.putShort(0xffff);
	//b.putString(rq->sink->get_remote_ip());
	b.putString(rq->sink->data.url->host);
	b.putShort(rq->sink->get_self_port());
	//is secure
	b.putByte(KBIT_TEST(rq->sink->data.url->flags, KGL_URL_SSL) ? 1 : 0);
	int64_t content_length = in->f->get_read_left(in, rq);
	KHttpHeader* header = rq->sink->data.GetHeader();
	int count = 0;
	while (header) {
		if (!is_internal_header(header)) {
			count++;
		}
		header = header->next;
	}
	if (rq->sink->data.if_modified_since != 0 || rq->sink->data.if_none_match != NULL) {
		count++;
	}
	if (content_length > 0) {
		count++;
	}
	b.putShort(count);
	//printf("head count=%d\n",count);
	header = rq->sink->data.GetHeader();
	bool founded;
	while (header) {
		if (is_internal_header(header)) {
			goto do_not_insert;
		}
		//printf("try send attr[%s] val[%s]\n",header->attr,header->val);
		founded = false;
		/*
		for(unsigned short i=1;i<MAX_AJP_REQUEST_HEADERS;i++){
			if (strcasecmp(ajp_request_headers[i],header->attr)==0) {
				i |= 0xA000;
				b.putShort(i);
				founded=true;
				break;
			}
		}
		//*/
		if (!founded) {
			b.putString(header->attr);
		}
		b.putString(header->val);
	do_not_insert:header = header->next;
	}
	if (content_length > 0) {
		b.putShort(0xA008);
		b.putString((char*)int2string(content_length, tmpbuff));
	}
	if (rq->sink->data.if_modified_since != 0) {
		char tmpbuff[50];
		mk1123time(rq->sink->data.if_modified_since, tmpbuff, sizeof(tmpbuff));
		if (rq->ctx->mt == modified_if_range_date) {
			b.putString("If-Range");
		} else {
			b.putString("If-Modified-Since");
		}
		b.putString(tmpbuff);
	} else if (rq->sink->data.if_none_match != NULL) {
		if (rq->ctx->mt == modified_if_range_etag) {
			b.putString("If-Range");
		} else {
			b.putString("If-None-Match");
		}
		b.putString(rq->sink->data.if_none_match->data, (int)rq->sink->data.if_none_match->len);
	}
	if (rq->sink->data.url->param) {
		//printf("send query_string\n");
		b.putByte(0x05);
		b.putString(rq->sink->data.url->param);
	}
	kgl_refs_string* param = client->get_param();
	if (param) {
		b.putByte(0x0C);
		b.putString(param->str.data, (int)param->str.len);
		release_string(param);
	}
	b.putByte(0xFF);
	b.end();
	return KGL_OK;
}
void KAjpFetchObject::BuildPostEnd()
{
	kbuf* ebuff = (kbuf*)xmalloc(sizeof(kbuf));
	ebuff->data = (char*)xmalloc(4);
	ebuff->used = 4;
	u_char* d = (u_char*)ebuff->data;
	d[0] = 0x12;
	d[1] = 0x34;
	d[2] = 0;
	d[3] = 0;
	buffer->Append(ebuff);
	//*/
}
KGL_RESULT KAjpFetchObject::ParseBody(KHttpRequest* rq, char** data, char* end)
{
	unsigned short chunk_length;
	char* chunk_data;
	unsigned char type;
	while (*data < end) {
		KAjpMessageParser msg;
		switch (parse(data, end, &msg)) {
		case kgl_parse_continue:
			return KGL_OK;
		case kgl_parse_success:
			break;
		default:
			return KGL_EDATA_FORMAT;
		}
		if (!msg.getByte(&type)) {
			return KGL_EDATA_FORMAT;
		}
		//printf("type=[%d]\n",type);
		switch (type) {
		case JK_AJP13_END_RESPONSE:
			ReadBodyEnd(rq);
			break;
		case JK_AJP13_SEND_BODY_CHUNK:
			if (!msg.getShort(&chunk_length)) {
				return KGL_EDATA_FORMAT;
			}
			//printf("chunk_length=[%d]\n",chunk_length);
			chunk_data = msg.getBytes();
			KGL_RESULT ret = PushBody(rq, out, chunk_data, chunk_length);
			if (KGL_OK != ret) {
				return ret;
			}
			break;
		}
	}
	return KGL_OK;
}
kgl_parse_result KAjpFetchObject::ParseHeader(KHttpRequest* rq, char** data, char* end)
{
	while (*data < end) {
		//printf("parse header len=[%d] ",*len);
		KAjpMessageParser msg;
		switch (parse(data, end, &msg)) {
		case kgl_parse_continue:
			return kgl_parse_continue;
		case kgl_parse_success:
			break;
		default:
			return kgl_parse_error;
		}
		unsigned char type = parseMessage(rq, rq->ctx->obj, &msg);
		//printf("type=[%d]\n",type);
		switch (type) {
		case JK_AJP13_SEND_HEADERS:
			//printf("parse_finished *len=[%d]\n",*len);
			return kgl_parse_finished;
		case JK_AJP13_GET_BODY_CHUNK:
			if (*data == end) {
				//BuildPostEnd();
				return kgl_parse_want_read;
			}
			break;
		default:
			klog(KLOG_ERR, "ajp unknow type=[%d] when parse header.\n", (int)type);
			break;
		}
	}
	return kgl_parse_continue;
}
unsigned char KAjpFetchObject::parseMessage(KHttpRequest* rq, KHttpObject* obj, KAjpMessageParser* msg)
{
	unsigned char type;
	if (!msg->getByte(&type)) {
		return JK_AJP13_ERROR;
	}
	switch (type) {
	case JK_AJP13_SEND_HEADERS:
		unsigned short status_code;
		if (!msg->getShort(&status_code)) {
			return JK_AJP13_ERROR;
		}
		char* status_msg;
		if (!msg->getString(&status_msg)) {
			return JK_AJP13_ERROR;
		}
		PushStatus(rq, status_code);
		unsigned short head_count;
		if (!msg->getShort(&head_count)) {
			return JK_AJP13_ERROR;
		}
		//printf("head count=%d\n",head_count);
		unsigned char head_type;
		for (int i = 0; i < head_count; i++) {
			if (!msg->peekByte(&head_type)) {
				return JK_AJP13_ERROR;
			}
			int attr_len = 0;
			int val_len = 0;
			const char* attr = NULL;
			char* val = NULL;
			if (head_type == 0xA0) {
				unsigned short head_attr;
				if (!msg->getShort(&head_attr)) {
					return JK_AJP13_ERROR;
				}
				head_attr = head_attr & 0xFF;
				if (head_attr > 0 && head_attr < MAX_AJP_RESPONSE_HEADERS) {
					kgl_str_t attr_s = ajp_response_headers[head_attr];
					attr = attr_s.data;
					attr_len = (int)attr_s.len;
				}

			} else {
				attr_len = msg->getString((char**)&attr);
				if (attr_len < 0) {
					return JK_AJP13_ERROR;
				}
			}
			val_len = msg->getString(&val);
			if (val_len < 0) {
				return JK_AJP13_ERROR;
			}
			//printf("attr=%s ",attr);
			//printf("val=%s\n",val);
			if (attr) {
				PushHeader(rq, attr, attr_len, val, val_len, false);
			}
		}
		break;
	case JK_AJP13_END_RESPONSE:
		ReadBodyEnd(rq);
		break;
	}
	return type;
}
//创建post数据到buffer中。
void KAjpFetchObject::buildPost(KHttpRequest* rq)
{
	unsigned len = buffer->getLen();
	//printf("buildpost len = %d\n",len);
	assert(len >= 0 && len <= AJP_PACKAGE);

	kbuf* nbuf = (kbuf*)malloc(sizeof(kbuf));
	nbuf->data = (char*)malloc(6);
	nbuf->used = 6;
	unsigned char* h = (unsigned char*)nbuf->data;
	h[0] = 0x12;
	h[1] = 0x34;
	unsigned dlen = len + 2;
	h[2] = (dlen >> 8 & 0xFF);
	h[3] = (dlen & 0xFF);
	h[4] = (len >> 8 & 0xFF);
	h[5] = (len & 0xFF);
	buffer->insertBuffer(nbuf);
	/*
	if (rq->sink->data.left_read == 0) {
		appendPostEnd();
	}
	//*/
}
kgl_parse_result KAjpFetchObject::parse(char** str, char* end, KAjpMessageParser* msg)
{
	//printf("parse len = %d\n",len);
	if (body_len == 0) {
		//head
		if (end - *str < 4) {
			return kgl_parse_continue;
		}
		unsigned char* ajp_header = (unsigned char*)*str;
		if (!((ajp_header[0] == 0x41 && ajp_header[1] == 0x42) || (ajp_header[0] == 0x12 && ajp_header[1] == 0x34))) {
			klog(KLOG_ERR, "recv wrong package.\n");
			return kgl_parse_error;
		}
		body_len = ajp_header[2] << 8 | ajp_header[3];
		if (body_len > AJP_PACKAGE) {
			klog(KLOG_ERR, "recv wrong package length %d.\n", body_len);
			return kgl_parse_error;
		}
		*str += 4;
	}
	if (end - *str < body_len) {
		return kgl_parse_continue;
	}
	msg->init(*str, body_len);
	*str += body_len;
	body_len = 0;
	return kgl_parse_success;
}
