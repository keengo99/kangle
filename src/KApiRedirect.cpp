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
#include "utils.h"
#include "KApiRedirect.h"
#include "log.h"
#include "do_config.h"
#include "KApiFetchObject.h"
#include "KHttpRequest.h"
#include "KVirtualHost.h"
#include "http.h"
#include "KApiFastcgiFetchObject.h"
#include "kselector_manager.h"
#include "KProcessManage.h"
#include "KSimulateRequest.h"
#include "KTimer.h"
#define   MAX_CRON_THREAD   5
#include "extern.h"
#include "ksapi.h"
#include "kmalloc.h"

#ifdef WHM_MODULE
#include "whm.h"
DLL_PUBLIC  BOOL   WINAPI Whm_GetExtensionVersion(HSE_VERSION_INFO* pVer);
DLL_PUBLIC  DWORD WINAPI Whm_HttpExtensionProc(EXTENSION_CONTROL_BLOCK* pECB);
DLL_PUBLIC  BOOL   WINAPI Whm_TerminateExtension(DWORD dwFlags);
#endif

using namespace std;
static void WINAPI apiServerFree(HCONN hConn,void *ptr) {
		free(ptr);
}
static BOOL WINAPI apiServerSupportFunction(HCONN hConn, DWORD dwHSERequest,
		LPVOID lpvBuffer, void **ret) {
	switch(dwHSERequest) {
	case KGL_REQ_SERVER_VAR:
		{
			const char *name = (const char *)lpvBuffer;
			if (name==NULL) {
				return FALSE;
			}
			const char *val = getSystemEnv(name);
			if (val==NULL || *val=='\0') {
				return FALSE;
			}
			(*ret) = (void *)strdup(val);
			return TRUE;
		}
	case KGL_REQ_CREATE_WORKER:
		{
			int *max_worker = (int *)lpvBuffer;
			kasync_worker *worker = kasync_worker_init(*max_worker,0);
			*ret = (void *)worker;
			return TRUE;
		}
	case KGL_REQ_RELEASE_WORKER:
		{
			kasync_worker *worker = (kasync_worker *)lpvBuffer;
			kasync_worker_release(worker);
			return TRUE;
		}
	case KGL_REQ_COMMAND:
		{
			kgl_command *command = (kgl_command *)lpvBuffer;
			Token_t token = NULL;
			if (command->vh) {
				KVirtualHost *vh = conf.gvm->refsVirtualHostByName(command->vh);
				if (vh==NULL) {
					return FALSE;
				}
#ifdef ENABLE_VH_RUN_AS
				bool result;
				token = vh->createToken(result);
				if (!result) {
					return FALSE;
				}
#endif
			}
			KCmdEnv *env = NULL;			
			if (command->env) {
				env = new KCmdEnv;
				kgl_command_env *ce = command->env;
				while (ce) {
					env->addEnv(ce->name,ce->val);
					ce = ce->next;
				}
			}
			pid_t pid = createProcess(token,command->cmd,env,command->dir,&command->std);
			if (token) {
				KVirtualHost::closeToken(token);
			}
			if (env) {
				delete env;
			}
			if (PID_LIKE(pid)) {
				*ret = (void *)(intptr_t)pid;
				return TRUE;
			}
			return FALSE;
		}
	case KGL_REQ_THREAD:
		{
			kgl_thread *thread = (kgl_thread *)lpvBuffer;
			if (thread->worker) {
				kasync_worker *worker = (kasync_worker *)thread->worker;
				if (kasync_worker_try_start(worker, thread->param, thread->thread_function,false)) {
					return TRUE;
				}
				return FALSE;
			}
			if (kasync_worker_thread_start(thread->param,thread->thread_function)) {
				return TRUE;
			}
			return FALSE;
		}
#if 0
	case KGL_REQ_TIMER:
		{
			kgl_timer *ctx = (kgl_timer *)lpvBuffer;
			timer_run(ctx->timer_run,ctx->arg,ctx->msec,ctx->selector);
			return TRUE;
		}
#endif
#ifdef ENABLE_SIMULATE_HTTP
	case KGL_REQ_ASYNC_HTTP:
		{
			kgl_async_http *ctx = (kgl_async_http *)lpvBuffer;
			if (kgl_simuate_http_request(ctx)==0) {
				return TRUE;
			}
			return FALSE;
		}
#endif
	default:
		return FALSE;
	}
	return FALSE;
}
KApiRedirect::KApiRedirect(const std::string &a): KRedirect(a) {
	life_time = 60;
	max_error_count = 5;
	type = WORK_TYPE_SP;
	dso.ConnID = (void *)this;
	dso.ServerSupportFunction = apiServerSupportFunction;
	dso.ServerFree = apiServerFree;
}
KApiRedirect::~KApiRedirect() {
	
}
void KApiRedirect::setFile(const std::string &file)
{
	apiFile = file;
	if (!isAbsolutePath(apiFile.c_str())) {
		dso.path = conf.path + apiFile;
	} else {
		dso.path = apiFile;
	}
}
#if 0
bool KApiRedirect::load(const std::string &file)
{
	apiFile = file;
	dso.path = dso.path + apiFile;
	return load();
}
#endif
bool KApiRedirect::parse_config(const khttpd::KXmlNode* node) {
	auto attr = node->attributes();
	auto type = attr("type",nullptr);
	if (type!=nullptr) {
		this->type = KApiRedirect::getTypeValue(type);
	}
	this->setFile(attr["file"]);
	return KExtendProgram::parse_config(node);
}
bool KApiRedirect::load()
{
#ifndef _WIN32
	if (strcasecmp(apiFile.c_str(),"bin/whm.so") == 0) {
		apiFile = "buildin:whm";
	}
#endif
	if (strncasecmp(apiFile.c_str(), "buildin:", 8) == 0) {
		dso.path = apiFile;
		dso.buildin = 1;
		type = WORK_TYPE_MT;
		if(strcasecmp(apiFile.c_str()+8,"whm")==0) {
#ifdef WHM_MODULE
			strcpy(dso.apiInfo,"whm");
			dso.GetExtensionVersion = Whm_GetExtensionVersion;
			dso.HttpExtensionProc = Whm_HttpExtensionProc;
			dso.TerminateExtension = Whm_TerminateExtension;
			bool result = dso.init();
			if(result){
				add_ref();
				conf.sysHost->addRedirect(true,"whm",this,"*", KConfirmFile::Exsit,"");
			}
			return result;
#else
			apiFile = "bin/whm.so";
			dso.path = "";
#endif
		}
	}
	if (!isAbsolutePath(apiFile.c_str())) {
		dso.path = conf.path + apiFile;
	}
	if(type==WORK_TYPE_MT) {
		bool result = dso.load();
		if (result) {
			if(strcmp(dso.apiInfo,"whm")==0){
				add_ref();
				conf.sysHost->addRedirect(true,"whm",this,"*", KConfirmFile::Exsit,"");
			}
		}
		return result;
    }
	return true;
}
KRedirectSource*KApiRedirect::makeFetchObject(KHttpRequest *rq, KFileName *file) {
	//设置api访问时使用full path_info
	KBIT_SET(rq->ctx.filter_flags,RQ_FULL_PATH_INFO);
	if (type == WORK_TYPE_MP || type == WORK_TYPE_SP) {
		return new KApiFastcgiFetchObject();
	}
	lock.Lock();
	if (dso.state == STATE_LOAD_UNKNOW) {
		load();
	}
	lock.Unlock();
	return new KApiFetchObject(this);
}
bool KApiRedirect::createProcess(KVirtualHost *vh, KPipeStream *st) {
	char *argv[2] = { (char *) conf.extworker.c_str(),NULL };
	bool result;
	Token_t token = NULL;
#ifdef ENABLE_VH_RUN_AS
	token = vh->getProcessToken(result);
	if (!result) {
		return false;
	}
#endif
	KCmdEnv *env = makeEnv(NULL);
	result = ::createProcess(st, token, argv, env, 0);
	if (env) {
		delete env;
	}
	KVirtualHost::closeToken(token);
	return result;
}
KUpstream* KApiRedirect::GetUpstream(KHttpRequest* rq)
{
	return spProcessManage.GetUpstream(rq, this);
}
bool KApiRedirect::unload() {
	KLocker locker(&lock);
	dso.unload();
	return true;
}