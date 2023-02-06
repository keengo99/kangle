/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include <string.h>
#include <stdlib.h>
#include <vector>
#include "KLang.h"
#include "KLangParser.h"
#include "KXml.h"
#include "do_config.h"
#include "kmalloc.h"
#include<iostream>
KLang klang;
using namespace std;
static const char *default_lang = "en";
KLang::KLang()
{
	langs = new map<char *,KLocalLang *,lessp_icase>;
}
KLang::~KLang()
{
}
void KLang::clear()
{
	std::map<char *,KLocalLang *,lessp_icase>::iterator it;
	for (it = langs->begin();it!=langs->end();it++) {
		free((*it).first);
		delete (*it).second;
	}
	delete langs;
}
bool KLang::load(std::string file)
{
	KXml xml;
	xml.setEvent(this);
	xml.parseFile(file);
	return true;
}
const char * KLang::operator[](const char *name)
{
	const char *value = get((char *)conf.lang,name);
	if(value==NULL && strcasecmp(conf.lang,default_lang)!=0){
		value = get((char *)default_lang,name);
	}
	if(value==NULL){
		return "";
	}
	return value;
}
const char *KLang::get(char *lang_name,const char *name)
{
	KLocalLang *ll = getLocal(lang_name);
	if(ll==NULL){
		return NULL;
	}
	return ll->get(name);
}
KLocalLang *KLang::getLocal(char *name)
{
	std::map<char *,KLocalLang *,lessp_icase>::iterator it;
	it = langs->find(name);
	if(it==langs->end()){
		return NULL;
	}
	return (*it).second;
}
bool KLang::startElement(KXmlContext *context) {
	if (context->qName == "include") {
		//KXml xml;
		KLang parser(langs);
		try {
			std::string file;
#ifdef KANGLE_WEBADMIN_DIR
			file = KANGLE_WEBADMIN_DIR;
#else
			file = conf.path + "/webadmin";
#endif
			file += "/";
			file += context->attribute["file"];
			parser.load(file.c_str());
		} catch(KXmlException &e) {
			fprintf(stderr,"catch exception :%s\n",e.what());
		}
		return true;
	}
	if (context->qName == "lang") {
		std::string name = context->attribute["name"];
		map<string,string>::iterator it;
		for (it= context->attribute.begin(); it!= context->attribute.end(); it++) {
			add(name,(*it).first,(*it).second);
		}
		return true;
	}
	//context->data = (void *)xstrdup(attribute["name"].c_str());
	return true;
}
bool KLang::startCharacter(KXmlContext *context, char *character,
		int len) {
	if (context->attribute["name"].size()==0) {
		return false;
	}
	const char *name = context->attribute["name"].c_str();
	std::map<char *,KLocalLang *,lessp_icase>::iterator it;
	it = langs->find((char *)name);
	string value;
	if(it!=langs->end()){
		value=replace(character, (*it).second->langs, "${", "}");
	}else{
		value = character;
	}
	return add(name,context->qName,value);
}
bool KLang::endElement(KXmlContext *context)
{
	return true;
}
bool KLang::add(std::string lang,std::string name,std::string value)
{
	if(lang.size()==0){
		//fprintf(stderr,"cann't add! lang name is zero\n");
		//return false;
		lang = default_lang;
	}
	std::map<char *,KLocalLang *,lessp_icase>::iterator it;
	it = langs->find((char *)lang.c_str());
	KLocalLang *ll = NULL;
	if(it==langs->end()){
		ll = new KLocalLang;
		langs->insert(pair<char *,KLocalLang *>(xstrdup(lang.c_str()),ll));
	}else{
		ll = (*it).second;
	}
	ll->add(name,value);
	return true;
}
void KLang::getAllLangName(std::vector<std::string> &names)
{
	std::map<char *,KLocalLang *,lessp_icase>::iterator it;
	for(it=langs->begin();it!=langs->end();it++){
		names.push_back((*it).first);
	}
}
