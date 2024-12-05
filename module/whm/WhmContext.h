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
#define OUTPUT_FORMAT_JSON  1
class WhmDataAttribute;
struct WhmDataValue {
	WhmDataValue(const KString& value,bool encode) {
		is_child = false;
		this->value = new std::vector<KString>{ value };
		this->encode = encode;
	}
	WhmDataValue();
	~WhmDataValue();
	void build(const KString &name,KWStream& s, int format);
	union {
		std::vector<KString>* value;
		WhmDataAttribute* attr;
	};
	bool is_child;
	bool encode;
};
class WhmDataAttribute : public KDataAttribute {
public:
	void add(const KString& name, const KString& value, bool encode = false) override {
		auto it = data.find(name);
		if (it == data.end()) {
			WhmDataValue* dv = new WhmDataValue(value, encode);
			data.insert(std::pair<KString, WhmDataValue*>(name, dv));
			return;
		}
		if ((*it).second->is_child) {
			return;
		}
		(*it).second->value->push_back(value);
	}
	KDataAttribute* add_child(const KString& name) override {
		auto it = data.find(name);
		if (it == data.end()) {
			WhmDataValue* dv = new WhmDataValue();
			data.insert(std::pair<KString, WhmDataValue*>(name, dv));
			return dv->attr;
		}
		if (!(*it).second->is_child) {
			return nullptr;
		}
		return (*it).second->attr;
	}
	void build(KWStream& s, int format) override {
		for (auto it = data.begin(); it != data.end(); ++it) {
			if (format == OUTPUT_FORMAT_JSON) {
				if (it != data.begin()) {
					s << ",";
				}
			}
			(*it).second->build((*it).first, s, format);				
		}		
	}
private:
	void add_data(const KString& name, WhmDataValue* value) {
		auto it = data.find(name);
		if (it != data.end()) {
			delete (*it).second;
		}
		data.insert(std::pair<KString, WhmDataValue*>(name, value));
	}
	std::map<KString, WhmDataValue*> data;
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
		if (statusMsg) {
			if(this->statusMsg.size()>0){
				this->statusMsg+="|";
			}
			this->statusMsg += statusMsg;
		}
	}
	bool flush(int status,int format = OUTPUT_FORMAT_XML);
	KDataAttribute* data() override {
		return &dv;
	}
	void add(const char *name, KString&value);
	void add(KString &name, KString&value);
	void add(const char *name,INT64 value);
	void add(const char* name, const char* value, bool encode=false);
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
		if(redirectCalls.size()>0){
			redirectCalls << ",";
		}
		redirectCalls << call;		
	}
	KStringStream*getRedirectCalls()
	{
		return &redirectCalls;
	}
	char * save(char *p);
private:
	WhmDataAttribute dv;
	std::list<char *> memorys;
	KServiceProvider *provider;
	KUrlValue urlValue;
	std::string statusMsg;
	KVirtualHost *vh;
	KExtendProgramString *ds;
	KStringStream redirectCalls;
};

#endif /* WHMCONTEXT_H_ */
