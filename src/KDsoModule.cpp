#include <assert.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include "KDsoModule.h"
#include "utils.h"
#include "log.h"

KDsoModule::KDsoModule()
{
	handle = NULL;
}
KDsoModule::~KDsoModule()
{
	
}
bool KDsoModule::isloaded()
{
	return handle!=NULL;
}
bool KDsoModule::load(const char *file)
{
	assert(handle==NULL);
	if(handle!=NULL){
		return false;
	}
	std::string path;
	if(!isAbsolutePath(file)){
		path = conf.path + file;
	}else{
		path = file;
	}
#ifdef _WIN32
	auto file_path = getPath(path.c_str());
	if (file_path != NULL) {
		SetDllDirectory(file_path.get());
	}
#endif
	handle = LoadLibrary(path.c_str());
	if (handle == NULL) {
		klog(KLOG_ERR,"cann't LoadLibrary %s %s\n", path.c_str(), getError());
		return false;
	}
#ifdef _WIN32
	SetDllDirectory(NULL);
#endif
	return true;
}
void *KDsoModule::findFunction(const char *func)
{
	if(handle==NULL){
		return NULL;
	}
	return GetProcAddress(handle,func);
}
const char *KDsoModule::getError()
{
#ifndef _WIN32
	return dlerror();
#else
	int err = GetLastError();
	klog(KLOG_ERR, "last errno=[%d]\n", err);
	return "";
#endif
}
bool KDsoModule::unload()
{
	if (handle == NULL) {
		return false;
	}
	FreeLibrary(handle);
	handle = NULL;
	return true;	
}
