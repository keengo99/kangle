/*
 * WhmContext.h
 *
 *  Created on: 2010-8-30
 *      Author: keengo
 */

#ifndef WHMCONTEXT_H_
#define WHMCONTEXT_H_
#include <string.h>
#include <string>
#include <list>
#include "KServiceProvider.h"
#include "KUrlValue.h"
#include "KVirtualHost.h"
#include "KStream.h"
#include "whm.h"
#include "KExtendProgram.h"
#define OUTPUT_FORMAT_XML   0
struct WhmAttribute {
	WhmAttribute()
	{
		encode = false;
	}
	std::string name;
	std::string value;
	bool encode;
};
class WhmContext : public KVirtualHostEvent ,public KDynamicString {
public:
	WhmContext(KServiceProvider *provider);
	virtual ~WhmContext();
	KWStream *getOutputStream() {
		return provider->getOutputStream();
	}
	KUrlValue *getUrlValue() {
		return &urlValue;
	}
	const char *getVhValue(const char *name)
	{
		if(vh==NULL || ds==NULL){
			return NULL;
		}
		const char *value = ds->interGetValue(name);
		if(value==NULL){
			return NULL;
		}
		return save(xstrdup(value));
		//return vh->
	}
	const char *getValue(const char *name)
	{
		if(strncmp(name,"url:",4)==0){
			return urlValue.getx(name+4);
		}
		if(strncmp(name,"vh:",3)==0){
			return (ds?ds->interGetValue(name+3):NULL);
		}
		const char *value = urlValue.getx(name);
		if(value){
			return value;
		}
		return (ds?ds->interGetValue(name):NULL);
	}
	Token_t getToken(bool &result) {
#ifdef HTTP_PROXY
		return NULL;
#else
		if (vh==NULL) {
			result = false;
			return NULL;
		}
		return vh->createToken(result);
#endif
	}
	KServiceProvider *getProvider() {
		return provider;
	}
	bool buildVh();
	void buildVh(KVirtualHost *vh);
	void setStatus(const char *statusMsg = NULL) {
		//this->status = status;
		if (statusMsg) {
			if(this->statusMsg.size()>0){
				this->statusMsg+="|";
			}
			this->statusMsg += statusMsg;
		}
	}
	bool flush(int status,int format = OUTPUT_FORMAT_XML);
	void add(const char *name, const char *value,bool encode=false);
	void add(const char *name,std::string &value);
	void add(std::string &name, std::string &value);
	void add(const char *name,INT64 value);
	KVirtualHost *getVh()
	{
		return vh;
	}
	bool hasVh()
	{
		return vh!=NULL;
	}
	void redirect(const char *call)
	{
		if(redirectCalls.getSize()>0){
			redirectCalls << ",";
		}
		redirectCalls << call;		
	}
	KStringBuf *getRedirectCalls()
	{
		return &redirectCalls;
	}
	char * save(char *p);
private:
	std::list<WhmAttribute *> attributes;
	std::list<char *> memorys;
	KServiceProvider *provider;
	KUrlValue urlValue;
	std::string statusMsg;
	KVirtualHost *vh;
	KExtendProgramString *ds;
	KStringBuf redirectCalls;
};

#endif /* WHMCONTEXT_H_ */
