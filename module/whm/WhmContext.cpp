/*
 * WhmContext.cpp
 *
 *  Created on: 2010-8-30
 *      Author: keengo
 */
#include "WhmContext.h"
using namespace std;

WhmContext::WhmContext(KServiceProvider *provider) {
	this->provider = provider;
	//status = WHM_OK;
	vh = NULL;
	ds = NULL;
}

WhmContext::~WhmContext() {
	list<WhmAttribute *>::iterator it;
	for (it = attributes.begin(); it != attributes.end(); it++) {
		delete (*it);
	}
	if(vh){
		vh->destroy();
	}
	if(ds){
		delete ds;
	}
	list<char *>::iterator it2;
	for(it2=memorys.begin();it2!=memorys.end();it2++){
		xfree((*it2));
	}
}
char * WhmContext::save(char *p)
{
	memorys.push_back(p);
	return p;
}
void WhmContext::add(const char *name,INT64 value)
{
	std::stringstream s;
	s << value;
	return add(name,s.str().c_str());
}
void WhmContext::add(const char *name,std::string &value)
{
	add(name,value.c_str());
}
void WhmContext::add(const char *name, const char *value,bool encode) {
	WhmAttribute *attribute = new WhmAttribute;
	attribute->name = name;
	attribute->value = value;
	attribute->encode = encode;
	attributes.push_back(attribute);
}
void WhmContext::add(std::string &name, std::string &value) {
	add(name.c_str(), value.c_str());
}
void WhmContext::buildVh(KVirtualHost *vh)
{
	if(this->vh){
		this->vh->destroy();
	}
	if(ds){
		delete ds;
		ds = NULL;
	}
	this->vh = vh;
	if(vh){
		ds = new KExtendProgramString("whm",vh);
	}
}
bool WhmContext::buildVh()
{

	const char *name = urlValue.getx("vh");
	if(name==NULL){
		return false;
	}
	if(this->vh){
		this->vh->destroy();
	}
	if(ds){
		delete ds;
		ds = NULL;
	}
	vh = conf.gvm->refsVirtualHostByName(name);
	if(vh){
		ds = new KExtendProgramString("whm",vh);
	}
	return true;
}
bool WhmContext::flush(int status,int format) {
	KWStream *out = getOutputStream();
	if (status > 0) {
		if (format == OUTPUT_FORMAT_XML) {
			*out << "<result status=\"" << status;
			if(statusMsg.size()>0){
				*out << " " << statusMsg;
			}
			*out << "\">\n";		
		}
	}
	list<WhmAttribute *>::iterator it;
	for (it = attributes.begin(); it != attributes.end(); it++) {
		if (format == OUTPUT_FORMAT_XML) {
			*out << "<" << (*it)->name.c_str() << ">" ;
			if ((*it)->encode) {
				*out << CDATA_START << (*it)->value.c_str() << CDATA_END;
			} else {
				*out << (*it)->value.c_str();
			}			
			*out << "</" << (*it)->name.c_str() << ">\n";
		}
		delete (*it);
	}
	attributes.clear();
	if (status > 0) {
		if (format == OUTPUT_FORMAT_XML) {
			*out << "</result>\n";
		}
	}
	return true;
}
