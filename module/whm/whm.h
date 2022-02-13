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
#ifndef WHM_H_
#define WHM_H_
#include "ksapi.h"
#ifdef __cplusplus
extern "C" {
#endif

#define WHM_EVENT_BEFORE   "before"
#define WHM_EVENT_AFTER    "after"
#define WHM_EVENT_CALL     "call"

#define WHM_UNKNOW           999
#define WHM_OK               200
#define WHM_PROGRESS         201
#define WHM_ASYNC_FAILED     202
#define WHM_REDIRECT         302
#define WHM_FORBIDEN         403
#define WHM_BAD_REQUEST      402
#define WHM_PARAM_ERROR      404
#define WHM_EVENT_FAILED     405
#define WHM_NEED_VH          406
#define WHM_PACKAGE_ERROR    407
#define WHM_CALL_FAILED      500
#define WHM_MODULE_NOT_FOUND 501
#define WHM_CALL_NOT_FOUND   502
#define WHM_COMMAND_FAILED   503
#define WHM_REQUEST_CMD      100
#define WHM_REQUEST_MAPFILE  101
#define CURRENT_WHM_VERSION  2
struct WHM_CMD_DATA
{
	void *ctx;
	BOOL async;//未实现异步功能
	BOOL runasuser;
	char **args;
	char **envs;
	void (* writeData)(void *ctx,const char *str,int len);
	void (* writeEnd)(void *ctx);
};
struct WHM_VERSION_INFO {
	int ver;
	const char *(* getEnv)(const char *name);
	BOOL (* ServerSupportFunction)(int request,void *buffer, int *size, int *dataType);
	char info[128];
};
struct WHM_CONTEXT 
{
	/*
	context,调用时需回传给kangle
	*/
	void *ctx;
	/*
	得到url变量
	*/
	const char *(* getUrl)(void *ctx,const char *name);
	/*
	得到虚拟主机变量
	*/
	const char *(* getVh)(void *ctx,const char *name);
	/*
	得到一些全局变量
	*/
	const char *(* getEnv)(void *ctx,const char *name);
	/*
	设置错误信息
	*/
	void (* setError)(void *ctx,const char *errMsg);
	/*
	设置返回值
	*/
	void (* setValue)(void *ctx,const char *name,const char *value);
	/*
	检测是否有虚拟主机
	*/
	BOOL (* hasVh)(void *ctx);
	void (* redirect)(void *ctx,const char *package,const char *call);
	BOOL (* ServerSupportFunction)(void *ctx, int request,void *buffer, int *size, int *dataType);
	//通过ServerSupportFunction分配的内存，要调用下面的函数来释放
	void (* free)(void *ctx,void *param);
};
DLL_PUBLIC BOOL WINAPI GetWhmVersion(WHM_VERSION_INFO *pVer);
DLL_PUBLIC int WINAPI WhmCall(const char *callName, const char *eventType,WHM_CONTEXT *context);
DLL_PUBLIC BOOL WINAPI WhmTerminate(DWORD dwFlags);
#ifdef __cplusplus
}
#endif
#ifndef WHM_MODULE
#define Whm_GetExtensionVersion GetExtensionVersion
#define Whm_HttpExtensionProc HttpExtensionProc
#define Whm_TerminateExtension TerminateExtension
#define WhmCoreCall WhmCall
#endif
int WINAPI WhmCoreCall(const char *callName, const char *event, WHM_CONTEXT *context);

#endif /* WHM_H_ */
