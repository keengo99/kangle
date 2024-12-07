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
#include "serializable.h"
#if 0
#define OUTPUT_FORMAT_XML   0
#define OUTPUT_FORMAT_JSON  1
class WhmDataAttribute;
enum class WhmDataType {
	STR,
	OBJ,
	INT
};
struct WhmDataValue {
	WhmDataValue(const KString& value, bool encode) {
		type = WhmDataType::STR;
		this->strs = new std::vector<KString>{ value };
		this->encode = encode;
	}
	WhmDataValue(int64_t v) :encode{ false } {
		type = WhmDataType::INT;
		this->ints = new std::vector<int64_t>{ v };
	}
	WhmDataValue();
	~WhmDataValue();
	void build(const KString& name, KWStream& s, int format);
	union {
		std::vector<KString>* strs;
		std::vector<WhmDataAttribute*>* objs;
		std::vector<int64_t>* ints;
	};
	WhmDataType type;
	bool encode;
};
class WhmDataAttribute : public KDataAttribute {
public:
	~WhmDataAttribute() {
		for (auto it = data.begin(); it != data.end(); ++it) {
			delete (*it).second;
		}
	}
	bool add(const KString& name, const KString& value, bool encode = false) override {
		auto it = data.find(name);
		if (it == data.end()) {
			WhmDataValue* dv = new WhmDataValue(value, encode);
			data.insert(std::pair<KString, WhmDataValue*>(name, dv));
			return true;
		}
		if ((*it).second->type != WhmDataType::STR) {
			return false;
		}
		(*it).second->strs->push_back(value);
		return true;
	}
	bool add(const KString& name, int64_t value) override {
		auto it = data.find(name);
		if (it == data.end()) {
			WhmDataValue* dv = new WhmDataValue(value);
			data.insert(std::pair<KString, WhmDataValue*>(name, dv));
			return true;
		}
		if ((*it).second->type != WhmDataType::INT) {
			return false;
		}
		(*it).second->strs->push_back(value);
		return true;
	}
	KDataAttribute* add(const KString& name) override {
		auto it = data.find(name);
		WhmDataValue* dv = nullptr;
		if (it == data.end()) {
			dv = new WhmDataValue();
			data.insert(std::pair<KString, WhmDataValue*>(name, dv));
		} else {
			dv = (*it).second;
		}
		if (dv->type != WhmDataType::OBJ) {
			return nullptr;
		}
		WhmDataAttribute* attr = new WhmDataAttribute();
		dv->objs->push_back(attr);
		return attr;
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
#endif
class WhmContext : public KVirtualHostEvent, public KDynamicString {
public:
	WhmContext(KServiceProvider* provider);
	virtual ~WhmContext();
	KWStream* getOutputStream() {
		return provider->getOutputStream();
	}
	KUrlValue* getUrlValue() {
		return &urlValue;
	}
	const char* getVhValue(const char* name) {
		if (vh == NULL || ds == NULL) {
			return NULL;
		}
		const char* value = ds->interGetValue(name);
		if (value == NULL) {
			return NULL;
		}
		return save(xstrdup(value));
	}
	const char* getValue(const char* name) {
		if (strncmp(name, "url:", 4) == 0) {
			return urlValue.getx(name + 4);
		}
		if (strncmp(name, "vh:", 3) == 0) {
			return (ds ? ds->interGetValue(name + 3) : NULL);
		}
		const char* value = urlValue.getx(name);
		if (value) {
			return value;
		}
		return (ds ? ds->interGetValue(name) : NULL);
	}
	Token_t getToken(bool& result) {
#ifdef HTTP_PROXY
		return NULL;
#else
		if (vh == NULL) {
			result = false;
			return NULL;
		}
		return vh->createToken(result);
#endif
	}
	KServiceProvider* getProvider() {
		return provider;
	}
	bool buildVh();
	void buildVh(KVirtualHost* vh);
	void setStatus(const char* statusMsg = NULL) {
		if (statusMsg) {
			if (this->statusMsg.size() > 0) {
				this->statusMsg += "|";
			}
			this->statusMsg += statusMsg;
		}
	}
	bool flush(int status, kgl::format fmt = kgl::format::xml_text);
	kgl::serializable* data() override {
		return &dv;
	}
	kgl::serializable* add(const KString& name) {
		return data()->add(name);
	}
	void add(const char* name, KString& value);
	bool add(const KString& name, const KString& value);
	bool add(const char* name, INT64 value);
	bool add(const char* name, const char* value, bool encode = false);
	KVirtualHost* getVh() {
		return vh;
	}
	bool hasVh() {
		return vh != NULL;
	}
	void redirect(const char* call) {
		if (redirectCalls.size() > 0) {
			redirectCalls << ",";
		}
		redirectCalls << call;
	}
	KStringStream* getRedirectCalls() {
		return &redirectCalls;
	}
	char* save(char* p);
private:
	kgl::serializable dv;
	std::list<char*> memorys;
	KServiceProvider* provider;
	KUrlValue urlValue;
	std::string statusMsg;
	KVirtualHost* vh;
	KExtendProgramString* ds;
	KStringStream redirectCalls;
};

#endif /* WHMCONTEXT_H_ */
