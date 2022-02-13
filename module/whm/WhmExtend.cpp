/*
 * WhmExtend.cpp
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */

#include "WhmExtend.h"
#include "KDynamicString.h"
#include "WhmPackage.h"
#include "WhmPackageManage.h"
using namespace std;
WhmExtend::WhmExtend() {

}

WhmExtend::~WhmExtend() {

}
bool WhmExtend::init(std::string &whmFile) {
	KDynamicString ds;
	char *dpath = ds.parseString(file.c_str());
	if(dpath){
		file = dpath;
		xfree(dpath);
	}
	if (!isAbsolutePath(file.c_str())) {
		char *path = getPath(whmFile.c_str());
		string newPath = path;
		xfree(path);
		newPath += PATH_SPLIT_CHAR;
		newPath += file;
		newPath.swap(file);
	}
	return true;
}
int WhmExtend::whmCall(const char *callName,const char *eventType,WhmContext *context)
{
	int ret = call(callName,eventType,context);
	KStringBuf *rd = context->getRedirectCalls();
	if(ret == WHM_OK){
		if(rd && rd->getSize()>0){
			//检查是否重定向
			ret = WHM_REDIRECT;
		}
	}
	if(ret != WHM_REDIRECT){
		//如果不是重定向，就返回结果
		if(rd){
			rd->clean();
		}
		return ret;
	}
	//重定向调用
	KServiceProvider *sp = context->getProvider();
	stringstream s;
	char *path = getPath(sp->getFileName());
	if(path == NULL){
		return WHM_CALL_FAILED;
	}
	char *buf = NULL;
	if(rd==NULL || rd->getSize()<=0){
		ret = WHM_CALL_FAILED;
	}else{
		buf = rd->stealString();
		char *hot = buf;
		rd->init(256);		
		for(;;){
			char *p = strchr(hot,',');
			if(p != NULL){
				*p = '\0';
			}
			char *p2 = strchr(hot,':');
			if(p2 == NULL){
				ret = WHM_PARAM_ERROR;
				break;
			}
			*p2 = '\0';
			s.str("");
			s << path << PATH_SPLIT_CHAR << hot;
			WhmPackage *package = packageManage.load(s.str().c_str());
			if(package == NULL){
				ret = WHM_PACKAGE_ERROR;
				break;
			} else {
				ret = package->process(p2+1,context);
				package->destroy();
			}
			if(p==NULL){
				break;
			}
			hot = p+1;
		}
	}
	if (buf) {
		xfree(buf);
	}
	if (path) {
		xfree(path);
	}
	return ret;	
}
