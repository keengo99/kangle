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
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <errno.h>
#include "log.h"
#include "utils.h"
#include "KApiDso.h"
#include "KDynamicString.h"

using namespace std;

KApiDso::KApiDso()
{
	flags = 0;
	handle = NULL;
	GetExtensionVersion = NULL;
	HttpExtensionProc = NULL;
	TerminateExtension = NULL;
	state = STATE_LOAD_UNKNOW;
	ConnID = NULL;
	ServerFree = NULL;
	ServerSupportFunction = NULL;
	memset(apiInfo, 0, sizeof(apiInfo));
}
KApiDso::~KApiDso()
{
	unload();
}
const char* KApiDso::getError() {
#ifndef _WIN32
	return dlerror();
#else
	return "";
#endif
}

bool KApiDso::reload() {
	unload();
	return load();
}
void KApiDso::unload() {
	BOOL terminate = TRUE;
	if (TerminateExtension) {
		debug("call Terminate\n");
		terminate = TerminateExtension(0);
		TerminateExtension = NULL;
	}
	if (handle) {
		debug("unload api [%s]\n", path.c_str());
		if (terminate) {
			FreeLibrary(handle);
		}
		handle = NULL;
		GetExtensionVersion = NULL;
		HttpExtensionProc = NULL;
	}
}
bool KApiDso::load(std::string file) {
	//apiFile = file;
	assert(handle == NULL);
	assert(GetExtensionVersion == NULL);
	path = file;
	return load();
}
bool KApiDso::init() {
	HSE_VERSION_INFO info;
	memset(&info, 0, sizeof(HSE_VERSION_INFO));
	info.dwExtensionVersion = HSE_VERSION;
	strncpy(info.hvcb.lpszExtensionDesc, path.c_str(),
		sizeof(info.hvcb.lpszExtensionDesc) - 1);
	info.hvcb.cbSize = sizeof(HSE_VERSION_CONTROL_BLOCK);
	info.hvcb.ConnID = this->ConnID;
	info.hvcb.ServerFree = this->ServerFree;
	info.hvcb.ServerSupportFunction = this->ServerSupportFunction;
	if (GetExtensionVersion(&info) == FALSE) {
		klog(KLOG_ERR, "call GetExtensionVersion return failed %s %d\n", path.c_str(), GetLastError());
		unload();
		return false;
	}
	snprintf(apiInfo, sizeof(apiInfo), "%d.%d", HIWORD(info.dwExtensionVersion), LOWORD(info.dwExtensionVersion));
	state = STATE_LOAD_SUCCESS;
	debug("load [%s] success\n", path.c_str());
	return true;
}

bool KApiDso::load() {
	if (STATE_LOAD_SUCCESS == state) {
		//已经成功，不可再加载
		return true;
	}
	state = STATE_LOAD_FAILED;
#ifdef _WIN32
	char* file_path = getPath(path.c_str());
	if (file_path != NULL) {
		SetDllDirectory(file_path);
		xfree(file_path);
	}
#endif
	KDynamicString ds;
	char* filename = ds.parseString(path.c_str());
	handle = LoadLibrary(filename);
	if (handle == NULL) {
		klog(KLOG_ERR, "cann't LoadLibrary %s %s\n", filename, getError());
		xfree(filename);
		return false;
	}
	xfree(filename);
#ifdef _WIN32
	SetDllDirectory(NULL);
#endif
	GetExtensionVersion = (GetExtensionVersionf)GetProcAddress(handle,
		"GetExtensionVersion");
	if (GetExtensionVersion == NULL) {
		debug("cann't find GetExtensionVersion function\n");
		unload();
		return false;
	}
	HttpExtensionProc = (HttpExtensionProcf)GetProcAddress(handle,
		"HttpExtensionProc");
	if (HttpExtensionProc == NULL) {
		debug("cann't find HttpExtensionProc function\n");
		unload();
		return false;
	}
	TerminateExtension = (TerminateExtensionf)GetProcAddress(handle,
		"TerminateExtension");
	return init();
}
