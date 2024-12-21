/*
 * WhmPackage.cpp
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */
#include <string.h>
#include "WhmPackage.h"
#include "WhmLog.h"
#include "WhmCommand.h"
#include "WhmDso.h"
#include "WhmUrl.h"
#include "WhmShell.h"
#include "WhmShellSession.h"
#include "WhmCore.h"

using namespace std;
WhmPackage::WhmPackage() {
	curCallable = NULL;
	curExtend = NULL;
}

WhmPackage::~WhmPackage() {
	for (auto it = callmap.begin(); it != callmap.end(); it++) {
		delete (*it).second;
	}
	if (curCallable) {
		delete curCallable;
	}
	std::map<KString, WhmExtend *>::iterator it2;
	for (it2 = extends.begin(); it2 != extends.end(); it2++) {
		delete (*it2).second;
	}
}
void WhmPackage::flush()
{
	std::map<KString, WhmExtend *>::iterator it;
	for (it = extends.begin(); it != extends.end(); it++) {
		(*it).second->flush();
	}
}
//query whm shell progress infomation
int WhmPackage::query(WhmContext *context)
{
	KString session = context->getUrlValue()->get("session");
	size_t pos = session.find(':');
	if (pos==KString::npos) {
		context->setStatus("session format is error");
		return WHM_PARAM_ERROR;
	}
	KString extend = session.substr(0,pos);
	std::map<KString, WhmExtend *>::iterator it = extends.find(extend);
	if (it==extends.end() || strcmp((*it).second->getType(),"shell")!=0) {
		context->setStatus("cann't find such session");
		return WHM_PARAM_ERROR;
	}
	WhmShell *shell = static_cast<WhmShell *>((*it).second);
	WhmShellContext *sc = shell->refsContext(session);
	if (sc==NULL) {
		context->setStatus("cann't find such session");
		return WHM_PARAM_ERROR;
	}
	KVirtualHost *vh = context->getVh();
	if (sc->vh==NULL && vh) {
		context->setStatus("access denied");
		return WHM_FORBIDEN;
	}
	if (sc->vh) {
		if (vh==NULL || vh->name!=sc->vh->name) {
			context->setStatus("access denied");
			return WHM_FORBIDEN;
		}
	}
	int ret = shell->result(sc,context);
	if (ret==WHM_OK) {
		shell->removeContext(sc);
	}
	sc->release();
	return ret;
}
//terminate a whm shell
int WhmPackage::terminate(WhmContext *context)
{
	context->setStatus("not implement");
	return WHM_CALL_FAILED;
}
int WhmPackage::getInfo(WhmContext *context)
{
	//context->setStatus(WHM_OK);
	context->add("version",version.c_str());
	for(auto it=extends.begin();it!=extends.end();it++){
		context->add("extend",(*it).first.c_str());
	}
	for(auto it2=callmap.begin();it2!=callmap.end();it2++){
		context->add("call",(*it2).first.c_str());
	}
	//context->flush(WHM_OK,OUTPUT_FORMAT_XML);
	return WHM_OK;
}
int WhmPackage::process(const char *callName,WhmContext *context) {
	if(strcasecmp(callName,"manifest")==0){
		return getInfo(context);
	}
	if (strcasecmp(callName,"query")==0) {
		return query(context);
	}
	if (strcasecmp(callName,"terminate")==0) {
		return terminate(context);
	}
	auto it = callmap.find(callName);
	if (it != callmap.end()) {
		return (*it).second->call(context);
	}
	return WHM_CALL_NOT_FOUND;
}
WhmExtend *WhmPackage::findExtend(const KString &name) {
	std::map<KString, WhmExtend *>::iterator it;
	it = extends.find(name);
	if (it == extends.end()) {
		return NULL;
	}
	return (*it).second;
}
WhmCallMap *WhmPackage::newCallMap(const KString &name, const KString &callName) {
	WhmExtend *extend = findExtend(name);
	if (extend == NULL) {
		return NULL;
	}
	return new WhmCallMap(extend, callName.empty()?name:callName);
}
bool WhmPackage::startElement(KXmlContext *context) {
	if(context->parent == NULL){
		//root element
		version = context->attribute["version"];
		return true;
	}
	if (context->qName == "call") {
		assert(curCallable==NULL);
		if (curCallable) {
			WhmError("curCall is not NULL\n");
		} else {
			auto  name = context->attribute["name"];
			auto extendCall = context->attribute["call"];
			if(extendCall.size()==0){
				extendCall = name;
			}
			curCallable = newCallMap(context->attribute["extend"],extendCall);
			if (curCallable == NULL) {
				WhmError("cann't find extend[%s]\n",
					context->attribute["extend"].c_str());
				return false;
			}
			curCallable->name = name;
			if (context->attribute["scope"] == "public") {
				curCallable->scope = callScopePublic;
			}
		}
	}
	if (context->qName == "event") {
		if (curCallable == NULL) {
			WhmError("cann't add event,it must under tag call\n");
		}
		WhmExtend *extend = findExtend(context->attribute["extend"]);
		if (extend == NULL) {
			WhmError("cann't find extend[%s]\n", context->attribute["extend"].c_str());
			return false;
		}
		if (strcasecmp(context->attribute["type"].c_str(), "before") == 0) {
			curCallable->addBeforeEvent(extend, context->attribute);
		} else {
			curCallable->addAfterEvent(extend, context->attribute);
		}
	} else if (context->qName == "extend") {
		KString name = context->attribute["name"];
		WhmExtend *extend = findExtend(name);
		if (extend) {
			WhmError("extend name[%s] is duplicate\n", name.c_str());
			return false;
		}
		KString type = context->attribute["type"];
		if (type.size()>0) {
			//have type attribute
			if (type=="shell") {
				extend = new WhmShell();
			} else if (type == "core") {
				extend = new WhmCore();
			}
		} else {
			//@deprecated please use type=<dso|cmd|url|whmshell>
			KString file;
			file = context->attribute["dso"];
			if (file.size() == 0) {
				file = context->attribute["cmd"];
				if (file.size() == 0) {
					file = context->attribute["url"];
					if (file.size() == 0) {
						WhmError("new extend error. type must be dso or cmd\n");
						return false;
					}
					extend = new WhmUrl(file);
				} else {
					WhmCommand *e = new WhmCommand(file);
					if (context->attribute["runas"] == "user") {
						e->runAsUser = true;
					} else {
						e->runAsUser = false;
					}
					extend = e;
				}
			} else {
				extend = new WhmDso(file);
			}
		}
		if (extend==NULL) {
			WhmError("extend type error.\n");
			return false;
		}
		extend->name = name;
		if (!extend->init(this->file)) {
			WhmError("cann't init extend[%s]\n", name.c_str());
		}
		extends.insert(std::pair<KString, WhmExtend *> (name, extend));
		curExtend = extend;
	} else if (context->qName=="include") {
		KString name = context->attribute["extend"];
		WhmExtend *extend = findExtend(name);
		if (extend==NULL) {
			WhmError("extend name[%s] cann't find\n",name.c_str());
			return false;
		}
		if(strcmp(extend->getType(),"shell")!=0){
			WhmError("only shell extend can be include\n");
			return false;
		}
		if (curExtend==NULL || strcmp(curExtend->getType(),"shell")!=0) {
			WhmError("only shell can be included in shell extend\n");
			return false;
		}
		WhmShell *shell = static_cast<WhmShell *>(curExtend);
		shell->include(static_cast<WhmShell *>(extend));		
	}
	if (curExtend) {
		curExtend->startElement(context);
	}
	return true;
}
bool WhmPackage::startCharacter(KXmlContext *context, char *character, int len)
{
	if (curExtend) {
		return curExtend->startCharacter(context,character,len);
	}
	return true;
}
bool WhmPackage::endElement(KXmlContext *context) {
	if (context->qName == "call") {
		if (curCallable) {
			if (findCallMap(curCallable->name)) {
				WhmError("call [%s] duplicate\n", curCallable->name.c_str());
				delete curCallable;
			} else {
				callmap.insert(std::pair<KString, WhmCallMap *> (curCallable->name, curCallable));
			}
			curCallable = NULL;
		}
	}
	if (context->qName == "extend") {
		curExtend = NULL;
	}
	if (curExtend) {
		curExtend->endElement(context);
	}
	return true;
}
WhmCallMap *WhmPackage::findCallMap(const KString &name) {
	std::map<KString, WhmCallMap *>::iterator it;
	it = callmap.find(name);
	if (it != callmap.end()) {
		return (*it).second;
	}
	return NULL;
}
