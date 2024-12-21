/*
 * WhmDso.cpp
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <errno.h>
#include "WhmDso.h"
#include "WhmLog.h"
#include "kforwin32.h"
struct WhmCreateProcessParam
{
	WHM_CMD_DATA* cmd;
	KPipeStream* st;
};
KTHREAD_FUNCTION createProcessThread(void* param) {
	WhmCreateProcessParam* cp = (WhmCreateProcessParam*)param;
	KPipeStream* st = cp->st;
	WHM_CMD_DATA* cmd = cp->cmd;
	char buf[512];
	for (;;) {
		int len = st->read(buf, sizeof(buf) - 1);
		if (len <= 0) {
			break;
		}
		if (cmd->writeData) {
			cmd->writeData(cmd->ctx, buf, len);
		} else {
			buf[len] = '\0';
			fprintf(stderr, "%s", buf);
		}
	}
	if (cmd->writeEnd) {
		cmd->writeEnd(cmd->ctx);
	}
	delete st;
	delete cp;
	KTHREAD_RETURN;
}
BOOL whmCreateProcess(WhmContext* ctx, WHM_CMD_DATA* cmd) {
	bool tokenResult;
	Token_t token = NULL;
	if (cmd->runasuser && ctx) {
		token = ctx->getToken(tokenResult);
		if (!tokenResult) {
			return FALSE;
		}
	}
	KCmdEnv* envs = make_cmd_env(cmd->envs);
	KPipeStream* st = createProcess(token, cmd->args, envs, 1);
	if (envs) {
		delete envs;
	}
	if (cmd->runasuser) {
		KVirtualHost::closeToken(token);
	}
	//BOOL result = FALSE;
	if (st) {
		WhmCreateProcessParam* param = new WhmCreateProcessParam;
		if (param == NULL) {
			delete st;
			return FALSE;
		}
		param->cmd = cmd;
		param->st = st;
		if (cmd->async) {
#ifndef _WIN32
			pthread_t id;
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);//设置线程为分离
#endif
			if (!PTHREAD_CREATE_SUCCESSED(pthread_create(&id, &attr, createProcessThread, (void*)param))) {
				delete st;
				delete param;
				return FALSE;
			}
		} else {
			createProcessThread((void*)param);
		}
		return TRUE;
	}
	return FALSE;
}
static BOOL globalWhmServerSupportFunction(int request, void* buffer, int* size, int* dataType) {
	if (request == WHM_REQUEST_CMD) {
		return whmCreateProcess(NULL, (WHM_CMD_DATA*)buffer);
	}
	return FALSE;
}
static BOOL whmServerSupportFunction(void* param, int request, void* buffer, int* size, int* dataType) {
	WhmContext* ctx = (WhmContext*)param;
	if (request == WHM_REQUEST_CMD) {
		return whmCreateProcess((WhmContext*)param, (WHM_CMD_DATA*)buffer);
	}
	if (request == WHM_REQUEST_MAPFILE) {
		KVirtualHost* vh = ctx->getVh();
		if (vh == NULL) {
			return FALSE;
		}
		char** d = (char**)dataType;
		*d = KFileName::concatDir(vh->doc_root.c_str(), (const char*)buffer);
		return TRUE;
	}
	return FALSE;
}
static const char* whmGetSystemEnv(const char* name) {
	static KString value;
	const char* value2 = getSystemEnv(name);
	if (value2) {
		return value2;
	}
	if (!conf.gvm->vhs.getEnvValue(name, value)) {
		return NULL;
	}
	return value.c_str();
}
static const char* whmGetUrlValue(void* param, const char* name) {
	WhmContext* ctx = (WhmContext*)param;
	KUrlValue* uv = ctx->getUrlValue();
	return uv->getx(name);
}
static const char* whmGetVhValue(void* param, const char* name) {
	WhmContext* ctx = (WhmContext*)param;
	return ctx->getVhValue(name);
}
static void whmSetError(void* param, const char* errMsg) {
	WhmContext* ctx = (WhmContext*)param;
	ctx->setStatus(errMsg);
}
static void whmSetValue(void* param, const char* name, const char* value) {
	WhmContext* ctx = (WhmContext*)param;
	ctx->add(name, value);
}
static const char* whmGetEnv(void* param, const char* name) {
	KString value;
	WhmContext* ctx = (WhmContext*)param;
	const char* value2 = getSystemEnv(name);
	if (value2) {
		return value2;
	}
	if (!conf.gvm->vhs.getEnvValue(name, value)) {
		return NULL;
	}
	return ctx->save(xstrdup(value.c_str()));
}
static BOOL whmHasVh(void* param) {
	WhmContext* ctx = (WhmContext*)param;
	if (ctx->hasVh()) {
		return TRUE;
	}
	return FALSE;
}
static void whmFree(void* ctx, void* ptr) {
	free(ptr);
}
WhmDso::WhmDso(KString& file) {
	this->file = file;
	handle = NULL;
	GetWhmVersion = NULL;
	WhmCall = NULL;
	WhmTerminate = NULL;
}
WhmDso::~WhmDso() {
	if (handle) {
		if (WhmTerminate) {
			WhmTerminate(0);
		}
		FreeLibrary(handle);
	}
}
bool WhmDso::init(KString& whmFile) {
	if (!WhmExtend::init(whmFile)) {
		return false;
	}
#ifdef _WIN32
	auto file_path = getPath(file.c_str());
	if (file_path) {
		SetDllDirectory(file_path.get());
	}
	file += ".dll";
#else
	file += ".so";
#endif
	handle = LoadLibrary(file.c_str());
	if (handle == NULL) {
		int last_error = GetLastError();
		WhmNotice("cann't LoadLibrary [%s] error=%d\n", file.c_str(), last_error);
		return false;
	}
#ifdef _WIN32
	SetDllDirectory(NULL);
#endif
	GetWhmVersion = (GetWhmVersionf)GetProcAddress(handle, "GetWhmVersion");
	if (GetWhmVersion == NULL) {
		WhmNotice("cann't find GetWhmVersion function [%s]\n", file.c_str());
		return false;
	}

	WhmCall = (WhmCallf)GetProcAddress(handle, "WhmCall");
	WhmTerminate = (WhmTerminatef)GetProcAddress(handle, "WhmTerminate");
	if (WhmCall == NULL) {
		WhmError("Cann't find Call function [%s]\n", file.c_str());
		return false;
	}
	WhmNotice("load [%s] success\n", file.c_str());
	WHM_VERSION_INFO info;
	memset(&info, 0, sizeof(info));
	info.ver = CURRENT_WHM_VERSION;
	info.getEnv = whmGetSystemEnv;
	info.ServerSupportFunction = globalWhmServerSupportFunction;
	return !!GetWhmVersion(&info);
}
int WhmDso::call(const char* callName, const char* eventType,
	WhmContext* context) {
	if (WhmCall == NULL) {
		return WHM_CALL_NOT_FOUND;
	}
	WHM_CONTEXT whmCtx;
	whmCtx.ctx = (void*)context;
	whmCtx.getUrl = whmGetUrlValue;
	whmCtx.getVh = whmGetVhValue;
	whmCtx.getEnv = whmGetEnv;
	whmCtx.setError = whmSetError;
	whmCtx.setValue = whmSetValue;
	whmCtx.hasVh = whmHasVh;
	whmCtx.ServerSupportFunction = whmServerSupportFunction;
	whmCtx.free = whmFree;
	return WhmCall(callName, eventType, &whmCtx);
}
