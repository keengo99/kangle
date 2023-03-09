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
#include "kserver.h"
#include "api_child.h"

#ifdef _WIN32
extern HANDLE api_child_token;
#endif
KApiFetchObject::KApiFetchObject(KApiRedirect* rd) :
	KApiService(&rd->dso) {
	rq = NULL;
	token = NULL;
	push_parser.parser.first_same = 1;
}

KApiFetchObject::~KApiFetchObject() {
	if (token) {
		KVirtualHost::closeToken(token);
	}
}
Token_t KApiFetchObject::getVhToken(const char* vh_name)
{
#ifndef HTTP_PROXY
	if (rq && rq->sink->data.opaque) {
		if (static_cast<KSubVirtualHost*>(rq->sink->data.opaque)->vh->user.empty()) {
			//只有超级账号运行的虚拟主机才有权限。
			KVirtualHost* vh = conf.gvm->refsVirtualHostByName(vh_name);
			if (vh) {
				if (token) {
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
	if (api_child_token) {
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
			token = (Token_t)&id;
#endif
			//KVirtualHost::createToken(token);
		}
	}
#endif
	return token;
#endif
	return NULL;
}
KGL_RESULT KApiFetchObject::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out)
{
	KHttpObject* obj = rq->ctx.obj;
	assert(dso);
	this->rq = rq;
	this->in = in;
	this->out = out;
	KGL_RESULT result = KGL_OK;
	if (dso->HttpExtensionProc) {
		assert(rq);
		if (brd->rd->is_disable()) {
			return out->f->error(out->ctx,  STATUS_SERVER_ERROR, _KS("extend is disable"));
		}
		KBIT_SET(obj->index.flags, ANSW_LOCAL_SERVER);
		if (rq->auth) {
			const char* auth_type = KHttpAuth::buildType(
				rq->auth->getType());
			const char* user = rq->auth->getUser();
			if (user) {
				env.addEnv("AUTH_USER", user);
			}
			env.addEnv("AUTH_TYPE", auth_type);
			if (rq->auth->getType() == AUTH_BASIC) {
				KHttpBasicAuth* auth = (KHttpBasicAuth*)rq->auth;
				const char* password = auth->getPassword();
				if (password) {
					env.addEnv("AUTH_PASSWORD", password);
				}
			}
		}
		bool chrooted = false;
#ifndef _WIN32
		chrooted = rq->get_virtual_host()->vh->chroot;
#endif
		make_http_env(rq, in, brd, rq->file, &env, chrooted);
		result = start();
	}
	if (!headSended && result == KGL_OK) {
		headSended = true;
		result = out->f->write_header_finish(out->ctx,  -1, &body);
	}
	if (body.ctx) {
		assert(result != KGL_NO_BODY);
		return body.f->close(body.ctx, result);
	}
	if (result == KGL_NO_BODY || no_body) {
		return result;
	}
	return result;
}
bool KApiFetchObject::initECB(EXTENSION_CONTROL_BLOCK* ecb) {
	memset(ecb, 0, sizeof(EXTENSION_CONTROL_BLOCK));
	ecb->cbSize = sizeof(EXTENSION_CONTROL_BLOCK);
	ecb->ConnID = (HCONN) static_cast<KApiService*>(this);
	ecb->dwVersion = MAKELONG(0, 6);
	ecb->lpszMethod = (LPSTR)(rq->get_method());
	ecb->lpszLogData[0] = '\0';
	ecb->lpszPathInfo = (char*)env.getEnv("PATH_INFO");
	ecb->lpszPathTranslated = (char*)env.getEnv("PATH_TRANSLATED");
	int64_t content_length = rq->get_left();
	ecb->cbTotalBytes = content_length;
	ecb->cbLeft = content_length;
	ecb->lpszContentType = (env.contentType ? env.contentType : (char*)"");
	ecb->dwHttpStatusCode = STATUS_OK;
	ecb->lpszQueryString = (char*)env.getEnv("QUERY_STRING");
	if (ecb->lpszQueryString == NULL) {
		ecb->lpszQueryString = (char*)"";
	}
	if (ecb->lpszPathTranslated == NULL) {
		ecb->lpszPathTranslated = (char*)"";
	}
	ecb->ServerSupportFunction = ServerSupportFunction;
	ecb->GetServerVariable = GetServerVariable;
	ecb->WriteClient = WriteClient;
	ecb->ReadClient = ReadClient;
	return true;
}
bool KApiFetchObject::setStatusCode(const char* status, int len) {
	//printf("status: %s\n",status);
	rq->ctx.obj->data->i.status_code = atoi(status);
	return true;
}
KGL_RESULT KApiFetchObject::map_url_path(const char* url, LPVOID file, LPDWORD file_len)
{
	char* filename = rq->map_url_path(url, this->brd);
	KGL_RESULT result = set_variable(file, file_len, filename);
	if (filename) {
		xfree(filename);
	}	
	return result;
}
KGL_RESULT KApiFetchObject::addHeader(const char* attr, int len) {

	if (len == 0) {
		len = (int)strlen(attr);
	}
	switch (push_parser.Parse(rq, attr, len)) {
	case kgl_parse_error:
		return KGL_EDATA_FORMAT;
	case kgl_parse_continue:
		return KGL_OK;
	case kgl_parse_finished:
	{
		headSended = true;
		auto result = out->f->write_header_finish(out->ctx, -1,  &body);
		if (result == KGL_NO_BODY) {
			no_body = true;
		}
		return result;
	}
	default:
		kassert(false);
		return KGL_EDATA_FORMAT;
	}
}
int KApiFetchObject::writeClient(const char* str, int len) {
	if (!headSended) {
		if (checkResponse(rq, rq->ctx.obj) == JUMP_DENY) {
			responseDenied = true;
		}
	}
	headSended = true;
	if (responseDenied) {
		return -1;
	}
	if (body.ctx == nullptr) {
		return -1;
	}
	if (body.f->write(body.ctx, str, len) != KGL_OK) {
		return -1;
	}
	return len;
}
int KApiFetchObject::readClient(char* buf, int len) {
	return in->f->body.read(in->body_ctx, buf, len);
}
