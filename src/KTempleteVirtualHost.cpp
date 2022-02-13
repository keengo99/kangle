/*
 * KTemplateVirtualHost.cpp
 *
 *  Created on: 2010-11-19
 *      Author: keengo
 */

#include "KTempleteVirtualHost.h"
using namespace std;

KTempleteVirtualHost::KTempleteVirtualHost() {

}

KTempleteVirtualHost::~KTempleteVirtualHost() {
	/*
	std::list<KExtendProgramEvent *>::iterator it;
	for (it = initEvents.begin(); it != initEvents.end(); it++) {
		delete (*it);
	}
	for (it = destroyEvents.begin(); it != destroyEvents.end(); it++) {
		delete (*it);
	}
	*/
}
void KTempleteVirtualHost::addEvent(const char *etag, std::map<std::string,
		std::string>&attribute) {
	std::string ev = attribute["event"];
	if (ev.size()>0) {
		if (strcasecmp(etag,"init_event")==0) {
			initEvents = ev;
		} else if(strcasecmp(etag,"destroy_event")==0) {
			destroyEvents = ev;
		} else if (strcasecmp(etag,"update_event")==0) {
			updateEvents = ev;
		}
	}
}
bool KTempleteVirtualHost::updateEvent(KVirtualHostEvent *ctx)
{
	bool result = false;
	if(tvh && tvh->updateEvent(ctx)){
		result = true;
	}
	if(updateEvents.size()>0){
		result = true;
		ctx->redirect(updateEvents.c_str());
	}
	return result;
}
bool KTempleteVirtualHost::initEvent(KVirtualHostEvent *ctx) {
	bool result = false;
	if(tvh && tvh->initEvent(ctx)){
		result = true;
	}
	if(initEvents.size()>0){
		result = true;
		ctx->redirect(initEvents.c_str());
	}
	return result;
}
bool KTempleteVirtualHost::destroyEvent(KVirtualHostEvent *ctx) {
	bool result = false;
	if(destroyEvents.size()>0){
		ctx->redirect(destroyEvents.c_str());
		result = true;
	}
	if(tvh && tvh->destroyEvent(ctx)){
		result = true;		
	}
	return result;
}
void KTempleteVirtualHost::buildXML(std::stringstream &s) {
	KVirtualHost::buildXML(s);
	if(initEvents.size()>0){
		s << "<init_event event='" << initEvents << "'/>\n";
	}
	if(destroyEvents.size()>0){
		s << "<destroy_event event='" << destroyEvents << "'/>\n";
	}
	if(updateEvents.size()>0){
		s << "<update_event event='" << updateEvents << "'/>\n";
	}
}
KGTempleteVirtualHost::KGTempleteVirtualHost()
{
	defaultTemplete = NULL;
}
KGTempleteVirtualHost::~KGTempleteVirtualHost()
{
	std::map<std::string,KTempleteVirtualHost *>::iterator it;
	for(it=tvhs.begin();it!=tvhs.end();it++){
		(*it).second->destroy();
	}
}
void KGTempleteVirtualHost::add(const char *subname,KTempleteVirtualHost *tvh,bool defaultFlag)
{

	std::map<std::string,KTempleteVirtualHost *>::iterator it;
	it = tvhs.find(subname);
	if(it != tvhs.end()){
		return;
	}
	tvhs.insert(pair<string,KTempleteVirtualHost *>(subname,tvh));
	tvh->addRef();
	if(defaultFlag){
		if(defaultTemplete == NULL){
			defaultTemplete = tvh;
		}
	}
}
KTempleteVirtualHost *KGTempleteVirtualHost::findTemplete(const char *subname,bool remove)
{
	KTempleteVirtualHost *tvh = NULL;
	if(subname==NULL){
		if(remove){
			return NULL;
		}
		tvh = getDefaultTemplete();
	}else{
		std::map<std::string,KTempleteVirtualHost *>::iterator it;
		it = tvhs.find(subname);
		if(it!=tvhs.end()){
			tvh = (*it).second;
			if(remove){
				tvhs.erase(it);
				if(tvh == defaultTemplete){
					defaultTemplete = NULL;
				}
			}
		}else{
			if(!remove){
				tvh = getDefaultTemplete();
			}
		}
	}
	if (tvh && !remove) {
		tvh->addRef();
	}
	return tvh;
}
KTempleteVirtualHost *KGTempleteVirtualHost::getDefaultTemplete()
{
	KTempleteVirtualHost *tvh = NULL;
	if(defaultTemplete){
		tvh = defaultTemplete;
		return tvh;
	}
	if(tvhs.size()>0){
		std::map<std::string,KTempleteVirtualHost *>::iterator it;
		it = tvhs.begin();
		tvh = (*it).second;
	}
	return tvh;
}

