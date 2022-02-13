/*
 * KApiFetchObject.cpp
 *
 *  Created on: 2010-6-13
 *      Author: keengo
 */
#include "http.h"
#include "KApiFetchObject.h"
#include "export.h"
#include "KHttpBasicAuth.h"
#include "KHttpProxyFetchObject.h"
#include "KHttpTransfer.h"
#ifdef _WIN32
extern HANDLE api_child_token;
#endif
KApiFetchObject::KApiFetchObject(KApiRedirect *rd) :
KApiService(&rd->dso) {
	rq = NULL;
	token = NULL;
	responseDenied = false;
	push_parser.parser.first_same = 1;
}

KApiFetchObject::~KApiFetchObject() {
	if (token) {
		KVirtualHost::closeToken(token);
	}
}
Token_t KApiFetchObject::getVhToken(const char *vh_name)
{
#ifndef HTTP_PROXY
	if(rq && rq->svh){
		if(rq->svh->vh->user.size()==0){
			//ֻ�г����˺����е�������������Ȩ�ޡ�
			KVirtualHost *vh = conf.gvm->refsVirtualHostByName(vh_name);
			if(vh){
				if(token){
					KVirtualHost::closeToken(token);
				}
				bool result;
				token = vh->createToken(result);
				return token;
			}
		}
	}
#endif
	return NULL;
}
Token_t KApiFetchObject::getToken() {
#if 0
#ifdef ENABLE_VH_RUN_AS
#ifdef _WIN32
	if(api_child_token){
		return api_child_token;
	}
#endif
	if (token == NULL) {
		if (rq && rq->svh) {
			bool result;
			//return rq->svh->vh->getLogonedToken(result);
			token = rq->svh->vh->createToken(result);
		} else {
#ifndef _WIN32
			token = (Token_t) &id;
#endif
			//KVirtualHost::createToken(token);
		}
	}
#endif
	return token;
#endif
	return NULL;
}
KGL_RESULT KApiFetchObject::Open(KHttpRequest *rq, kgl_input_stream* in, kgl_output_stream* out)
{
	KHttpObject *obj = rq->ctx->obj;
	assert(dso);
	this->rq = rq;
	if (dso->HttpExtensionProc) {
		assert(rq);
		if (!brd->rd->enable) {
			return out->f->write_message(out, rq, KGL_MSG_ERROR, "extend is disable", STATUS_SERVER_ERROR);
		}
		SET(obj->index.flags,ANSW_LOCAL_SERVER);
		if (rq->auth) {
			const char *auth_type = KHttpAuth::buildType(
					rq->auth->getType());
			const char *user = rq->auth->getUser();
			if (user) {
				env.addEnv("AUTH_USER", user);
			}
			env.addEnv("AUTH_TYPE", auth_type);
			if (rq->auth->getType() == AUTH_BASIC) {
				KHttpBasicAuth *auth = (KHttpBasicAuth *) rq->auth;
				const char *password = auth->getPassword();
				if (password) {
					env.addEnv("AUTH_PASSWORD", password);
				}
			}
		}
		bool chrooted = false;
#ifndef _WIN32
		chrooted = rq->svh->vh->chroot;
#endif
		make_http_env(rq, in, brd,rq->ctx->lastModified, rq->file, &env,chrooted);
		start();
	}
	return out->f->write_end(out, rq, KGL_OK);
}
bool KApiFetchObject::initECB(EXTENSION_CONTROL_BLOCK *ecb) {
	memset(ecb, 0, sizeof(EXTENSION_CONTROL_BLOCK));
	ecb->cbSize = sizeof(EXTENSION_CONTROL_BLOCK);
	ecb->ConnID = (HCONN) static_cast<KApiService *>(this);
	ecb->dwVersion = MAKELONG( 0, 6);
	ecb->lpszMethod = (LPSTR) (rq->getMethod());
	ecb->lpszLogData[0] = '\0';
	ecb->lpszPathInfo = (char *) env.getEnv("PATH_INFO");
	ecb->lpszPathTranslated = (char *) env.getEnv("PATH_TRANSLATED");
	int64_t content_length = rq->content_length;//gate->f->get_post_left(gate, rq);
	ecb->cbTotalBytes = content_length;
	ecb->cbLeft = content_length;
	ecb->lpszContentType = (env.contentType ? env.contentType : (char *) "");
	ecb->dwHttpStatusCode = STATUS_OK;
	ecb->lpszQueryString = (char *) env.getEnv("QUERY_STRING");
	if (ecb->lpszQueryString == NULL) {
		ecb->lpszQueryString = (char *) "";
	}
	ecb->ServerSupportFunction = ServerSupportFunction;
	ecb->GetServerVariable = GetServerVariable;
	ecb->WriteClient = WriteClient;
	ecb->ReadClient = ReadClient;
	return true;
}
bool KApiFetchObject::setStatusCode(const char *status, int len) {
	//printf("status: %s\n",status);
	rq->ctx->obj->data->status_code = atoi(status);
	return true;
}
bool KApiFetchObject::addHeader(const char *attr, int len) {

	if (len == 0) {
		len = (int)strlen(attr);
	}
	switch (push_parser.Parse(rq,attr,len)) {
	case kgl_parse_error:
		return false;
	case kgl_parse_continue:
	case kgl_parse_finished:
		return true;
	default:
		kassert(false);
		return false;
	}
}
int KApiFetchObject::writeClient(const char *str, int len) {
	if (!headSended) {
		if (checkResponse(rq,rq->ctx->obj) == JUMP_DENY) {
			responseDenied = true;
		}
	}
	headSended = true;
	if (responseDenied) {
		return -1;
	}
	if (rq->ctx->st == NULL) {
		prepare_write_stream(rq);
	}
	switch (rq->ctx->st->write_all(rq, str, len)) {
	case STREAM_WRITE_FAILED:
		return -1;
	default:
		break;
	}
	if (rq->tr->TrySyncWrite(rq)) {
		return len;
	}
	return -1;
}
int KApiFetchObject::readClient(char *buf, int len) {
	return rq->Read(buf, len);
}