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

using namespace std;
WhmPackage::WhmPackage() {
	curCallable = NULL;
	curExtend = NULL;
}

WhmPackage::~WhmPackage() {
	std::map<string, WhmCallMap *>::iterator it;
	for (it = callmap.begin(); it != callmap.end(); it++) {
		delete (*it).second;
	}
	if (curCallable) {
		delete curCallable;
	}
	std::map<std::string, WhmExtend *>::iterator it2;
	for (it2 = extends.begin(); it2 != extends.end(); it2++) {
		delete (*it2).second;
	}
}
void WhmPackage::flush()
{
	std::map<std::string, WhmExtend *>::iterator it;
	for (it = extends.begin(); it != extends.end(); it++) {
		(*it).second->flush();
	}
}
//query whm shell progress infomation
int WhmPackage::query(WhmContext *context)
{
	std::string session = context->getUrlValue()->get("session");
	size_t pos = session.find_first_of(':');
	if (pos==std::string::npos) {
		context->setStatus("session format is error");
		return WHM_PARAM_ERROR;
	}
	std::string extend = session.substr(0,pos);
	std::map<std::string, WhmExtend *>::iterator it = extends.find(extend);
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
	std::map<string, WhmExtend *>::iterator it;
	for(it=extends.begin();it!=extends.end();it++){
		context->add("extend",(*it).first.c_str());
	}
	std::map<std::string, WhmCallMap *>::iterator it2;
	for(it2=callmap.begin();it2!=callmap.end();it2++){
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
	std::map<string, WhmCallMap *>::iterator it;
	it = callmap.find(callName);
	if (it != callmap.end()) {
		int ret = (*it).second->call(context);
		//context->setStatus(ret);
		//context->flush(ret,OUTPUT_FORMAT_XML);
		return ret;
	}
	//KWStream *out = context->getOutputStream();
	//*out << "<result status=\"" << WHM_CALL_NOT_FOUND << "\"/>";
	return WHM_CALL_NOT_FOUND;
}
WhmExtend *WhmPackage::findExtend(std::string &name) {
	std::map<string, WhmExtend *>::iterator it;
	it = extends.find(name);
	if (it == extends.end()) {
		return NULL;
	}
	return (*it).second;
}
WhmCallMap *WhmPackage::newCallMap(std::string &name, std::string &callName) {
	if (callName.size() == 0) {
		callName = name;
	}
	WhmExtend *extend = findExtend(name);
	if (extend == NULL) {
		return NULL;
	}
	return new WhmCallMap(extend, callName);
}
bool WhmPackage::startElement(KXmlContext *context, std::map<std::string,
		std::string> &attribute) {
	if(context->parent == NULL){
		//root element
		version = attribute["version"];
		return true;
	}
	if (context->qName == "call") {
		assert(curCallable==NULL);
		if (curCallable) {
			WhmError("curCall is not NULL\n");
		} else {
			std::string name = attribute["name"];
			std::string extendCall = attribute["call"];
			if(extendCall.size()==0){
				extendCall = name;
			}
			curCallable = newCallMap(attribute["extend"],extendCall);
			if (curCallable == NULL) {
				WhmError("cann't find extend[%s]\n",
						attribute["extend"].c_str());
				return false;
			}
			curCallable->name = name;
			if (attribute["scope"] == "public") {
				curCallable->scope = callScopePublic;
			}
		}
	}
	if (context->qName == "event") {
		if (curCallable == NULL) {
			WhmError("cann't add event,it must under tag call\n");
		}
		WhmExtend *extend = findExtend(attribute["extend"]);
		if (extend == NULL) {
			WhmError("cann't find extend[%s]\n", attribute["extend"].c_str());
			return false;
		}
		if (strcasecmp(attribute["type"].c_str(), "before") == 0) {
			curCallable->addBeforeEvent(extend, attribute);
		} else {
			curCallable->addAfterEvent(extend, attribute);
		}
	} else if (context->qName == "extend") {
		std::string name = attribute["name"];
		WhmExtend *extend = findExtend(name);
		if (extend) {
			WhmError("extend name[%s] is duplicate\n", name.c_str());
			return false;
		}
		std::string type = attribute["type"];
		if (type.size()>0) {
			//have type attribute
			if (type=="shell") {
				extend = new WhmShell();
			}
		} else {
			//@deprecated please use type=<dso|cmd|url|whmshell>
			std::string file;
			file = attribute["dso"];
			if (file.size() == 0) {
				file = attribute["cmd"];
				if (file.size() == 0) {
					file = attribute["url"];
					if (file.size() == 0) {
						WhmError("new extend error. type must be dso or cmd\n");
						return false;
					}
					extend = new WhmUrl(file);
				} else {
					WhmCommand *e = new WhmCommand(file);
					if (attribute["runas"] == "user") {
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
		extends.insert(std::pair<std::string, WhmExtend *> (name, extend));
		curExtend = extend;
	} else if (context->qName=="include") {
		std::string name = attribute["extend"];
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
		curExtend->startElement(context,attribute);
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
				callmap.insert(std::pair<string, WhmCallMap *> (
						curCallable->name, curCallable));
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
WhmCallMap *WhmPackage::findCallMap(std::string &name) {
	std::map<std::string, WhmCallMap *>::iterator it;
	it = callmap.find(name);
	if (it != callmap.end()) {
		return (*it).second;
	}
	return NULL;
}
