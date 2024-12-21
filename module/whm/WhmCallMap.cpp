/*
 * WhmCallMap.cpp
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */
#include <string>
#include "kforwin32.h"
#include "WhmCallMap.h"
#include "kmalloc.h"
#include "WhmPackageManage.h"

using namespace std;
WhmCallMap::WhmCallMap(WhmExtend *callable, const KString &callName) {
	this->callable = new WhmExtendCall;
	this->callable->callName = callName;
	this->callable->extend = callable;
	scope = callScopePrivate;
}
WhmCallMap::~WhmCallMap() {
	if (callable) {
		delete callable;
	}
	std::list<WhmExtendCall *>::iterator it;
	for (it = beforeEvents.begin(); it != beforeEvents.end(); it++) {
		delete (*it);
	}
	for (it = afterEvents.begin(); it != afterEvents.end(); it++) {
		delete (*it);
	}
}
void WhmCallMap::addBeforeEvent(WhmExtend *event, KXmlAttribute&attribute) {
	WhmExtendCall *ec = makeEvent(attribute);
	if(ec){
		ec->extend = event;
		beforeEvents.push_back(ec);
	}
}
void WhmCallMap::addAfterEvent(WhmExtend *event, KXmlAttribute &attribute) {
	WhmExtendCall *ec = makeEvent(attribute);
	if(ec){
		ec->extend = event;
		afterEvents.push_back(ec);
	}
}
int WhmCallMap::call(WhmContext *context) {
	int ret;
	std::list<WhmExtendCall *>::iterator it;
	for (it = beforeEvents.begin(); it != beforeEvents.end(); it++) {
		ret = (*it)->whmCall(WHM_EVENT_BEFORE, context);
		if (ret != WHM_OK && !(*it)->force) {
			return ret;
		}
	}
	ret = callable->whmCall(WHM_EVENT_CALL,	context);
	for (it = afterEvents.begin(); it != afterEvents.end(); it++) {
		if(ret != WHM_OK && !(*it)->force){
			return ret;
		}
		ret = (*it)->whmCall( WHM_EVENT_AFTER, context);
		if (ret != WHM_OK) {
			return ret;
		}
	}
	return ret;
}
WhmExtendCall *WhmCallMap::makeEvent(KXmlAttribute&attribute)
{
	auto callName = attribute["call"];
	WhmExtendCall *ec = new WhmExtendCall;
	if (callName.size() == 0) {
		ec->callName = callable->callName;
	} else {
		ec->callName = callName;
	}
	if(attribute["force"] == "1"){
		ec->force = true;
	}else{
		ec->force = false;
	}
	return ec;
}

int WhmExtendCall::whmCall(const char* eventType, WhmContext* context) {
	if (!call_ptr) {
		call_ptr = extend->parse_call(callName);
	}
	int ret;
	if (call_ptr) {
		ret = (extend->*call_ptr)(callName.c_str(), eventType, context);
	} else {
		ret = extend->call(callName.c_str(), eventType, context);
	}
	KStringStream* rd = context->getRedirectCalls();
	if (ret == WHM_OK) {
		if (rd && rd->size() > 0) {
			//检查是否重定向
			ret = WHM_REDIRECT;
		}
	}
	if (ret != WHM_REDIRECT) {
		//如果不是重定向，就返回结果
		if (rd) {
			rd->clear();
		}
		return ret;
	}
	//重定向调用
	KServiceProvider* sp = context->getProvider();
	stringstream s;
	auto path = getPath(sp->getFileName());
	if (!path) {
		return WHM_CALL_FAILED;
	}
	kgl_auto_cstr buf;
	if (rd == NULL || rd->size() <= 0) {
		ret = WHM_CALL_FAILED;
	} else {
		buf = rd->steal();
		char* hot = buf.get();
		//rd->init(256);
		for (;;) {
			char* p = strchr(hot, ',');
			if (p != NULL) {
				*p = '\0';
			}
			char* p2 = strchr(hot, ':');
			if (p2 == NULL) {
				ret = WHM_PARAM_ERROR;
				break;
			}
			*p2 = '\0';
			s.str("");
			s << path.get() << PATH_SPLIT_CHAR << hot;
			WhmPackage* package = packageManage.load(s.str().c_str());
			if (package == NULL) {
				ret = WHM_PACKAGE_ERROR;
				break;
			} else {
				ret = package->process(p2 + 1, context);
				package->release();
			}
			if (p == NULL) {
				break;
			}
			hot = p + 1;
		}
	}
	return ret;
}
