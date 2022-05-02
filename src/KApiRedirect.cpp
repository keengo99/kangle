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
				*ret = (void *)pid;
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
KApiRedirect::KApiRedirect() {

	lifeTime = 60;
	max_error_count = 5;
	type = WORK_TYPE_AUTO;
	delayLoad = false;
	dso.ConnID = (void *)this;
	dso.ServerSupportFunction = apiServerSupportFunction;
	dso.ServerFree = apiServerFree;
}
KApiRedirect::~KApiRedirect() {
	
}
void KApiRedirect::setFile(std::string file)
{
	apiFile = file;
	dso.path = dso.path + apiFile;
}
bool KApiRedirect::load(std::string file)
{
	apiFile = file;
	dso.path = dso.path + apiFile;
	return load();
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
		if(strcasecmp(apiFile.c_str()+8,"whm")==0) {
#ifdef WHM_MODULE
			strcpy(dso.apiInfo,"whm");
			dso.GetExtensionVersion = Whm_GetExtensionVersion;
			dso.HttpExtensionProc = Whm_HttpExtensionProc;
			dso.TerminateExtension = Whm_TerminateExtension;
			bool result = dso.init();
			if(result){
				addRef();
				conf.sysHost->addRedirect(true,"whm",this,"*",KGL_CONFIRM_FILE_EXSIT,"");
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
	if(type==WORK_TYPE_MT || type==WORK_TYPE_AUTO) {
		bool result = dso.load();
		if (result) {
			if(strcmp(dso.apiInfo,"whm")==0){
				addRef();
				conf.sysHost->addRedirect(true,"whm",this,"*", KGL_CONFIRM_FILE_EXSIT,"");
			}
		}
		return result;
    }
	return true;
}
KFetchObject *KApiRedirect::makeFetchObject(KHttpRequest *rq, KFileName *file) {
	//设置api访问时使用full path_info
	KBIT_SET(rq->filter_flags,RQ_FULL_PATH_INFO);
	if (type == WORK_TYPE_MP || type == WORK_TYPE_SP) {
		return new KApiFastcgiFetchObject();
	}
	lock.Lock();
	if (dso.state == STATE_LOAD_UNKNOW) {
		load();
	}
	lock.Unlock();
	//return NULL;
	return new KApiFetchObject(this);
}
void KApiRedirect::buildXML(std::stringstream &s) {
	s << "\t<api name='" << name << "' file='" << this->apiFile << "' ";
	if (type != WORK_TYPE_AUTO) {
		s << "type='" << getTypeString(type) << "' ";
	}
	if (!enable) {
		s << "flag='disable' ";
	}
	if (delayLoad) {
		s << "delay_load='1' ";
	}
	KExtendProgram::buildConfig(s);
	s << "\t</api>\n";
}
bool KApiRedirect::createProcess(KVirtualHost *vh, KPipeStream *st) {
	char *argv[2] = { (char *) conf.extworker.c_str(),NULL };
	bool result;
	Token_t token = NULL;
	//st->process.detach();
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
