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
#include "lib.h"
#include "lang.h"
#include "KHttpResponseParser.h"
//#include "KHttpTransfer.h"
#include "KHttpBasicAuth.h"
#include "KApiRedirect.h"
#include "kmalloc.h"
#define PTR_LEN(end,start) ((u_char *)end - (u_char *)start)
KFastcgiFetchObject::KFastcgiFetchObject()
{
	body_len = -1;
	bodyEnd = false;
	pad_buf = NULL;
}
KFastcgiFetchObject::~KFastcgiFetchObject() {
	if (pad_buf) {
		xfree(pad_buf);
	}
}
KGL_RESULT KFastcgiFetchObject::buildHead(KHttpRequest *rq)
{
	parser.first_same = 1;
	pop_header.proto = Proto_fcgi;
	buffer = new KSocketBuffer(NBUFF_SIZE);
	KHttpObject *obj = rq->ctx->obj;
	KBIT_SET(obj->index.flags,ANSW_LOCAL_SERVER);
	KFastcgiStream<KSocketBuffer> fbuf;
	fbuf.setStream(buffer);
	fbuf.extend = isExtend();
	if (fbuf.extend) {
		FCGI_BeginRequestRecord package;
		memset(&package, 0, sizeof(package));
		package.header.type = FCGI_BEGIN_REQUEST;
		package.header.contentLength = htons(sizeof(FCGI_BeginRequestBody));
		KApiRedirect *ard = static_cast<KApiRedirect *>(brd->rd);
		package.body.id = ard->id;
		buffer->write_all(NULL, (char *)&package,sizeof(FCGI_BeginRequestRecord));
	}else{
		fbuf.beginRequest(client->GetLifeTime()>0);
	}
	if (isExtend() && rq->auth) {
		const char *auth_type = KHttpAuth::buildType(rq->auth->getType());
		const char *user = rq->auth->getUser();
		if (user) {
			fbuf.addEnv("AUTH_USER", user);
		}
		fbuf.addEnv("AUTH_TYPE", auth_type);
		if (rq->auth->getType() == AUTH_BASIC) {
			KHttpBasicAuth *auth = (KHttpBasicAuth *) rq->auth;
			const char *password = auth->getPassword();
			if (password) {
				fbuf.addEnv("AUTH_PASSWORD", password);
			}
		}
	}
	bool chrooted = false;
#ifndef _WIN32
	chrooted = rq->svh->vh->chroot;
#endif
	bool sendResult = make_http_env(rq, in, brd, rq->sink->data.if_modified_since, rq->file, &fbuf, chrooted);
	if (!sendResult) {//send error
		buffer->destroy();
		return KGL_EUNKNOW;
	}
	if (!rq->has_post_data()) {
		appendPostEnd();
	}
	return KGL_OK;
}
kgl_parse_result KFastcgiFetchObject::ParseHeader(KHttpRequest *rq, char **data, int *len)
{
	while (*len > 0) {
		int packet_len = *len;
		char *packet = parse(rq, data, len,&packet_len);
		if (packet && packet_len > 0) {
			RestorePacket(&packet, &packet_len);
			if (this->buf.type == FCGI_STDERR) {
				fwrite(packet, packet_len, 1, stderr);
				fwrite("\n", 1, 1, stderr);
			} else {
				kgl_parse_result ret = KAsyncFetchObject::ParseHeader(rq, &packet, &packet_len);
				switch (ret) {
				case kgl_parse_finished:
					if (pad_buf) {
						xfree(pad_buf);
						pad_buf = NULL;
					}
					body_len += packet_len;
					*len += packet_len;
					*data -= packet_len;
					return kgl_parse_finished;
				case kgl_parse_error:
					return kgl_parse_error;
				default:
					SavePacket(packet, packet_len);
					break;
				}
			}
		}
		if (bodyEnd) {
			return kgl_parse_error;
		}
	}
	return kgl_parse_continue;
}
KGL_RESULT KFastcgiFetchObject::ParseBody(KHttpRequest *rq, char **data, int *len)
{
	int packet_len;
	while (*len > 0) {
		char *packet = parse(rq, data, len,&packet_len);
		if (packet == NULL) {
			readBodyEnd(rq, data, len);
			break;
		}
		KGL_RESULT result = KAsyncFetchObject::ParseBody(rq, &packet, &packet_len);
		if (KGL_OK != result) {
			return result;
		}
	}
	return KGL_OK;
}
void KFastcgiFetchObject::appendPostEnd()
{
	if(isExtend()){
		//api使用不用发结束post标记
		return;
	}
	//最后的post数据
	kbuf *fcgibuff = (kbuf *)malloc(sizeof(kbuf));
	fcgibuff->data = (char *)malloc(sizeof(FCGI_Header));
	fcgibuff->used = sizeof(FCGI_Header);
	FCGI_Header *fcgiheader = (FCGI_Header *)fcgibuff->data;
	memset(fcgiheader, 0, sizeof(FCGI_Header));
	fcgiheader->version = 1;
	fcgiheader->type = FCGI_STDIN;
	fcgiheader->contentLength = 0;
	fcgiheader->requestIdB0 = 1;
	buffer->Append(fcgibuff);
}
void KFastcgiFetchObject::buildPost(KHttpRequest *rq)
{
	unsigned postLen = buffer->getLen();
	assert(postLen>0);
	kbuf *fcgibuff = (kbuf *)malloc(sizeof(kbuf));
	fcgibuff->data = (char *)malloc(sizeof(FCGI_Header));
	//nbuff *fcgibuff = (nbuff *)malloc(sizeof(nbuff) + sizeof(FCGI_Header));
	fcgibuff->used = sizeof(FCGI_Header);
	FCGI_Header *fcgiheader = (FCGI_Header *)fcgibuff->data;
	memset(fcgiheader, 0, sizeof(FCGI_Header));
	fcgiheader->version = 1;
	fcgiheader->type = FCGI_STDIN;
	fcgiheader->contentLength = htons(postLen);
	fcgiheader->requestIdB0 = 1;
	buffer->insertBuffer(fcgibuff);
	if(!rq->has_post_data()){
		appendPostEnd();
	}
}
char *KFastcgiFetchObject::parse(KHttpRequest *rq,char **str,int *len,int *packet_len)
{
	if (body_len==0) {
		if (buf.paddingLength>0) {
			//skip padding
			int padlen = MIN(*len,(int)buf.paddingLength);
			buf.paddingLength -= padlen;
			*str += padlen;
			*len -= padlen;
			if(buf.paddingLength>0){
				return NULL;
			}
		}
		body_len = -1;
	}
	if (body_len==-1) {
		//head
		if (*len < (int)sizeof(FCGI_Header)) {
			return NULL;
		}		
		kgl_memcpy(((char *)&buf),*str,sizeof(FCGI_Header));
		(*str) += sizeof(FCGI_Header);
		(*len) -= sizeof(FCGI_Header);		
		body_len = ntohs(buf.contentLength);
	}
	//printf("type=%d,body_len=%d,len=%d\n",buf.type,body_len,len);
	if (buf.type == FCGI_END_REQUEST) {
		expectDone(rq);
		bodyEnd = true;
		return NULL;
	}
	if (buf.type == FCGI_ABORT_REQUEST) {
		bodyEnd = true;
		return NULL;
	}
	*packet_len = MIN(*len,body_len);
	body_len -= *packet_len;
	char *body = *str;
	*str += *packet_len;
	*len -= *packet_len;	
	if (*packet_len ==0) {
		return NULL;
	}
	return body;
}

