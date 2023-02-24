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

using namespace std;
WhmCallMap::WhmCallMap(WhmExtend *callable, const std::string &callName) {
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
void WhmCallMap::addBeforeEvent(WhmExtend *event, std::map<std::string,std::string> &attribute) {
	WhmExtendCall *ec = makeEvent(attribute);
	if(ec){
		ec->extend = event;
		beforeEvents.push_back(ec);
	}
}
void WhmCallMap::addAfterEvent(WhmExtend *event, std::map<std::string,std::string> &attribute) {
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
		ret = (*it)->extend->whmCall((*it)->callName.c_str(), WHM_EVENT_BEFORE,
				context);
		if (ret != WHM_OK && !(*it)->force) {
			return ret;
		}
	}
	ret = callable->extend->whmCall(callable->callName.c_str(), WHM_EVENT_CALL,
			context);
	for (it = afterEvents.begin(); it != afterEvents.end(); it++) {
		if(ret != WHM_OK && !(*it)->force){
			return ret;
		}
		ret = (*it)->extend->whmCall((*it)->callName.c_str(), WHM_EVENT_AFTER, context);
		if (ret != WHM_OK) {
			return ret;
		}
	}
	return ret;
}
WhmExtendCall *WhmCallMap::makeEvent(std::map<std::string,std::string> &attribute)
{
	std::string callName = attribute["call"];
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
