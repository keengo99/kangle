/*
 * KFastcgiFetchObject.cpp
 *
 *  Created on: 2010-4-21
 *      Author: keengo
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include "do_config.h"

#include "KFastcgiFetchObject.h"
#include "KFastcgiUtils.h"
#include "http.h"
#include "KHttpLib.h"
#include "lang.h"
#include "KHttpResponseParser.h"
 //#include "KHttpTransfer.h"
#include "KHttpBasicAuth.h"
#include "KApiRedirect.h"
#include "kmalloc.h"
#define PTR_LEN(end,start) ((u_char *)end - (u_char *)start)
KFastcgiFetchObject::KFastcgiFetchObject()
{
	flags = 0;
	split_header = nullptr;
}
KFastcgiFetchObject::~KFastcgiFetchObject() {
	if (split_header) {
		ks_buffer_destroy(split_header);
	}
}
KGL_RESULT KFastcgiFetchObject::buildHead(KHttpRequest* rq)
{
	parser.first_same = 1;
	pop_header.proto = Proto_fcgi;
	buffer = new KSocketBuffer(NBUFF_SIZE);
	KHttpObject* obj = rq->ctx->obj;
	KBIT_SET(obj->index.flags, ANSW_LOCAL_SERVER);
	KFastcgiStream<KSocketBuffer> fbuf(FCGI_STDIN);
	fbuf.setStream(buffer);
	fbuf.extend = is_extend();
	if (fbuf.extend) {
		FCGI_BeginRequestRecord package;
		memset(&package, 0, sizeof(package));
		package.header.type = FCGI_BEGIN_REQUEST;
		package.header.contentLength = htons(sizeof(FCGI_BeginRequestBody));
		KApiRedirect* ard = static_cast<KApiRedirect*>(brd->rd);
		package.body.id = ard->id;
		buffer->write_all((char*)&package, sizeof(FCGI_BeginRequestRecord));
	} else {
		fbuf.beginRequest(client->GetLifeTime() > 0);
	}
	if (is_extend() && rq->auth) {
		const char* auth_type = KHttpAuth::buildType(rq->auth->getType());
		const char* user = rq->auth->getUser();
		if (user) {
			fbuf.addEnv("AUTH_USER", user);
		}
		fbuf.addEnv("AUTH_TYPE", auth_type);
		if (rq->auth->getType() == AUTH_BASIC) {
			KHttpBasicAuth* auth = (KHttpBasicAuth*)rq->auth;
			const char* password = auth->getPassword();
			if (password) {
				fbuf.addEnv("AUTH_PASSWORD", password);
			}
		}
	}
	bool chrooted = false;
#ifndef _WIN32
	chrooted = rq->get_virtual_host()->vh->chroot;
#endif
	bool sendResult = make_http_env(rq, in, brd, rq->sink->data.if_modified_since, rq->file, &fbuf, chrooted);
	if (!sendResult) {//send error
		buffer->destroy();
		return KGL_EUNKNOW;
	}

	if (!rq->has_post_data(in)) {
		appendPostEnd();
	}
	return KGL_OK;
}
kgl_parse_result KFastcgiFetchObject::ParseHeader(KHttpRequest* rq, char** pos, char* end)
{
	
	while (*pos < end) {
		char* piece = parse_fcgi_header(pos, end, true);
		if (piece == NULL) {
			return kgl_parse_continue;
		}
		int packet_length = *pos - piece;
		switch (fcgi_header_type) {
		case FCGI_STDERR:
			fwrite(piece, packet_length, 1, stderr);
			fwrite("\n", 1, 1, stderr);
			break;
		case API_CHILD_MAP_PATH:
		{

			char* url = (char*)malloc(packet_length + 1);
			kgl_memcpy(url, piece, packet_length);
			url[packet_length] = '\0';
			char* filename = rq->map_url_path(url, this->brd);
			int len = 0;
			if (filename) {
				len = (int)strlen(filename);
			}
			//printf("try write map_path_result filename=[%s].\n",filename?filename:"");
			KFastcgiStream<KUpstream> fbuf(client,FCGI_STDIN);
			if (!fbuf.write_data(API_CHILD_MAP_PATH_RESULT, filename, len)) {
				klog(KLOG_ERR, "write map_path_result failed.\n");
			}
			if (filename) {
				xfree(filename);
			}
			break;
		}
		case FCGI_ABORT_REQUEST:
		case FCGI_END_REQUEST:
			return kgl_parse_error;
		default:
		{
			char* piece_end = *pos;
			//fwrite(piece, 1, *data - piece, stdout);
			if (split_header != NULL) {
				ks_write_str(split_header, piece, packet_length);
				piece = split_header->buf;
				piece_end = piece + split_header->used;
			}
			//fwrite(piece, 1, piece_end - piece, stdout);
			kgl_parse_result ret = KAsyncFetchObject::ParseHeader(rq, &piece, piece_end);
			if (split_header != NULL) {
				if (piece == piece_end) {
					ks_buffer_destroy(split_header);
					split_header = NULL;
				} else {
					ks_save_point(split_header, piece);
				}
			} else if (piece < piece_end) {
				//still have data, save to split_header
				split_header = ks_buffer_new(NBUFF_SIZE);
				ks_write_str(split_header, piece, piece_end - piece);
			}
			//printf("parse header result=[%d]\n", ret);
			switch (ret) {
			case kgl_parse_finished:
				assert(*pos <= end);
				return kgl_parse_finished;
			case kgl_parse_continue:
				break;
			default:
				return kgl_parse_error;
			}
		}
		}
	}
	return kgl_parse_continue;
}
KGL_RESULT KFastcgiFetchObject::read_body_end(KHttpRequest* rq, KGL_RESULT result)
{
	if (!checkContinueReadBody(rq)) {
		return result;
	}
	KAsyncFetchObject::ReadBody(rq);
	return result;
}
KGL_RESULT KFastcgiFetchObject::ParseBody(KHttpRequest* rq, char** pos, char* end)
{
	KGL_RESULT result;
	if (split_header) {
		char* hot = split_header->buf;
		result = KAsyncFetchObject::ParseBody(rq, &hot, hot + split_header->used);
		//assert all data are drained
		assert(hot == split_header->buf + split_header->used);
		ks_buffer_destroy(split_header);
		split_header = nullptr;
		if (result != KGL_OK) {
			return result;
		}
	}
	assert(pop_header.recved_end_request == 0);
	while (*pos < end) {
		char* piece = parse_fcgi_header(pos, end, false);
		if (piece == NULL) {
			return KGL_OK;
		}
		//printf("fcgi_header_type=[%d]\n", fcgi_header_type);
		switch (fcgi_header_type) {
		case FCGI_END_REQUEST:
			expectDone(rq);
			pop_header.recved_end_request = 1;
			break;
		case FCGI_ABORT_REQUEST:
			pop_header.recved_end_request = 1;
			return KGL_EABORT;
		case FCGI_STDOUT:
			result = KAsyncFetchObject::ParseBody(rq, &piece, *pos);
			if (result != KGL_OK) {
				return result;
			}
			//assert all data are drained
			assert(piece == *pos);
			break;
		default:
			klog(KLOG_ERR, "recv unknow fastcgi type=[%d]\n", fcgi_header_type);
			break;
		}
	}
	return KGL_OK;
}
void KFastcgiFetchObject::appendPostEnd()
{
	//最后的post数据
	kbuf* fcgibuff = (kbuf*)malloc(sizeof(kbuf));
	fcgibuff->data = (char*)malloc(sizeof(FCGI_Header));
	fcgibuff->used = sizeof(FCGI_Header);
	FCGI_Header* fcgiheader = (FCGI_Header*)fcgibuff->data;
	memset(fcgiheader, 0, sizeof(FCGI_Header));
	fcgiheader->version = 1;
	fcgiheader->type = FCGI_STDIN;
	fcgiheader->contentLength = 0;
	fcgiheader->requestIdB0 = 1;
	buffer->Append(fcgibuff);
}
void KFastcgiFetchObject::buildPost(KHttpRequest* rq)
{
	unsigned postLen = buffer->getLen();
	assert(postLen > 0);
	kbuf* fcgibuff = (kbuf*)malloc(sizeof(kbuf));
	fcgibuff->data = (char*)malloc(sizeof(FCGI_Header));
	//nbuff *fcgibuff = (nbuff *)malloc(sizeof(nbuff) + sizeof(FCGI_Header));
	fcgibuff->used = sizeof(FCGI_Header);
	FCGI_Header* fcgiheader = (FCGI_Header*)fcgibuff->data;
	memset(fcgiheader, 0, sizeof(FCGI_Header));
	fcgiheader->version = 1;
	fcgiheader->type = FCGI_STDIN;
	fcgiheader->contentLength = htons(postLen);
	fcgiheader->requestIdB0 = 1;
	buffer->insertBuffer(fcgibuff);
	if (!rq->has_post_data(in)) {
		appendPostEnd();
	}
}
char* KFastcgiFetchObject::parse_fcgi_header(char** str, char* end, bool full)
{
	if (body_len == 0) {
		size_t packet_length = end - *str;
		if (fcgi_pad_length > 0) {
			if (packet_length < fcgi_pad_length) {
				return NULL;
			}
			packet_length -= fcgi_pad_length;
			*str += fcgi_pad_length;
			fcgi_pad_length = 0;
		}
		if (packet_length < sizeof(FCGI_Header)) {
			return NULL;
		}
		packet_length -= sizeof(FCGI_Header);
		FCGI_Header* header = (FCGI_Header*)(*str);
		uint16_t content_length = ntohs(header->contentLength);
		if (header->type == FCGI_END_REQUEST) {
			full = true;
		}
		if (full &&  packet_length < content_length + header->paddingLength) {
			//need full
			return NULL;
		}
		fcgi_header_type = header->type;
		fcgi_pad_length = header->paddingLength;
		//save fcgi_header;
		//kgl_memcpy(((char*)&fcgi_header), header, sizeof(FCGI_Header));
		body_len = content_length;
		//printf("fastcgi type=[%d] body_length=[%d] pad=[%d]\n", fcgi_header_type,body_len, header->paddingLength);
		(*str) += sizeof(FCGI_Header);
		//(*str) += header->paddingLength;
	}
	size_t len = end - *str;
	int packet_length = MIN((int)len, body_len);
	body_len -= packet_length;
	char* body = (*str);
	(*str) += packet_length;
	return body;
}

