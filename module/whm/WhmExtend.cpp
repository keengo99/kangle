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
bool WhmExtend::init(KString &whmFile) {
	KDynamicString ds;
	auto dpath = ds.parseString(file.c_str());
	if(dpath){
		file = dpath.get();
	}
	if (!isAbsolutePath(file.c_str())) {
		auto path = getPath(whmFile.c_str());
		KStringBuf newPath;
		newPath << path.get();
		newPath << PATH_SPLIT_CHAR << file;
		newPath.str().swap(file);
	}
	return true;
}
int WhmExtend::whmCall(const char *callName,const char *eventType,WhmContext *context)
{
	int ret = call(callName,eventType,context);
	KStringStream *rd = context->getRedirectCalls();
	if(ret == WHM_OK){
		if(rd && rd->size()>0){
			//����Ƿ��ض���
			ret = WHM_REDIRECT;
		}
	}
	if(ret != WHM_REDIRECT){
		//��������ض��򣬾ͷ��ؽ��
		if(rd){
			rd->clear();
		}
		return ret;
	}
	//�ض������
	KServiceProvider *sp = context->getProvider();
	stringstream s;
	auto path = getPath(sp->getFileName());
	if(!path){
		return WHM_CALL_FAILED;
	}
	kgl_auto_cstr buf;
	if(rd==NULL || rd->size()<=0){
		ret = WHM_CALL_FAILED;
	}else{
		buf = rd->steal();
		char *hot = buf.get();
		//rd->init(256);
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
			s << path.get() << PATH_SPLIT_CHAR << hot;
			WhmPackage *package = packageManage.load(s.str().c_str());
			if(package == NULL){
				ret = WHM_PACKAGE_ERROR;
				break;
			} else {
				ret = package->process(p2+1,context);
				package->release();
			}
			if(p==NULL){
				break;
			}
			hot = p+1;
		}
	}
	return ret;	
}
