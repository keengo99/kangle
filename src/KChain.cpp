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
#include <stdio.h>
#include "KAccess.h"
#include "KChain.h"
#include "KModel.h"
#include "lang.h"
//#include "KFilter.h"
#include "KModelManager.h"
#include "KTable.h"
#include "KHttpLib.h"
#include<list>
#include "kmalloc.h"
using namespace std;

KChain::KChain() {
	hit_count = 0;
	jump = NULL;
	jumpType = JUMP_CONTINUE;
	curacl = NULL;
	curmark = NULL;
	ext = cur_config_ext;
	next = NULL;
	prev = NULL;
}
KChain::~KChain() {
	clear();
}
void KChain::clear()
{
	if (jump) {
		jump->release();
		jump = NULL;
	}
	for (std::list<KAcl *>::iterator it = acls.begin(); it != acls.end(); it++) {
		KAccess::releaseRunTimeModel((*it));	
	}
	for (std::list<KMark *>::iterator it2 = marks.begin(); it2 != marks.end(); it2++) {
		KAccess::releaseRunTimeModel((*it2));
	}
	acls.clear();
	marks.clear();
}
bool KChain::match(KHttpRequest *rq, KHttpObject *obj, int &jumpType,
		KJump **jumpTable) {
	bool result = true;
	bool last_or = false;
	//OR NEXT
	for (std::list<KAcl *>::iterator it = acls.begin(); it != acls.end(); it++) {
		if (result && last_or) {
			last_or = (*it)->is_or;
			continue;
		}
		result = ((*it)->match(rq, obj) != (*it)->revers);
		last_or = (*it)->is_or;
		if (!result && !last_or) {
			break;
		}
	}
	if (!result) {
		return false;
	}
	last_or = false;
	std::list<KMark *>::iterator it2;
	jumpType = this->jumpType;
	*jumpTable = this->jump;
	for (it2 = marks.begin(); it2 != marks.end(); it2++) {
		if (result && last_or) {
			last_or = (*it2)->is_or;
			continue;
		}
		result = ((*it2)->mark(rq, obj, this->jumpType, jumpType) != (*it2)->revers);
		if (jumpType==JUMP_FINISHED) {
			jumpType = JUMP_DENY;
			result = true;
			break;
		}
		if (!result && !last_or) {
			break;
		}
		last_or = (*it2)->is_or;	
	}
	if (result) {
		hit_count++;
	}
	return result;
}
void KChain::getModelHtml(KModel *model, std::stringstream &s, int type,
		int index) {

	s << "<tr><td>";
	s << "<input type=hidden name='begin_sub_form' value='" << model->getName()
			<< "'>";
	//if (type==0) {
	s << "<input type=checkbox name='or' value='1' ";
	if (model->is_or) {
		s << "checked";
	}
	s << ">OR NEXT";
	s << "<input type=checkbox name='revers' value='1' ";
	if (model->revers) {
		s << "checked";
	}
	s << ">" << LANG_REVERS;

	//}
	s << "[<a href=\"javascript:delmodel('" << index << "'," << type << ");\">del</a>]";
	s << "[<a href=\"javascript:downmodel('" << index << "'," << type << ");\">down</a>]";
	s << model->getName() << "</td><td>";
	s << model->getHtml(model);
	s << "<input type=hidden name='end_sub_form' value='1'></td></tr>\n";
}
void KChain::getEditHtml(std::stringstream &s,u_short accessType) {

	std::map<std::string,KAcl *>::iterator itf;
	std::list<KAcl *>::iterator it;
	string revers;
	int index = 0;
	s << "<tr><td>name</td><td><input type='input' name='name' value='" << name << "' ";
	if (name.size()>0) {
		s << "readonly";
	}
	s << "></td></tr>";
	for (it = acls.begin(); it != acls.end(); it++) {
		getModelHtml((*it), s, 0, index);
		index++;
	}
	s << "<tr><td>" << klang["available_acls"] << "</td><td>";
	s
			<< "<select onChange='addmodel(this.options[this.options.selectedIndex].value,0)'><option value=''>";
	s  << klang["select_acls"] << "</option>";
	for (itf = kaccess->aclFactorys[accessType].begin(); itf
			!= kaccess->aclFactorys[accessType].end(); itf++) {
				s << "<option value='" << (*itf).first << "'>" << (*itf).first
				<< "</option>";
	}
	s << "</select>\n";
	//s << "<table><div id='acls' class='acls'>test</div></table>\n";
	s << "</td></tr>\n";
	std::map<std::string,KMark *>::iterator itf2;
	std::list<KMark *>::iterator it2;
	index = 0;
	for (it2 = marks.begin(); it2 != marks.end(); it2++) {
		getModelHtml((*it2), s, 1, index);
		index++;
	}
	//显示mark
	s << "<tr><td>" << klang["available_marks"] << "</td><td>";
	s << "<select onChange='";
	s << "addmodel(this.options[this.options.selectedIndex].value,1)'>";
	s << "<option value=''>" << klang["select_marks"] << "</option>";
	for (itf2 = kaccess->markFactorys[accessType].begin(); itf2
			!= kaccess->markFactorys[accessType].end(); itf2++) {
		s << "<option value='" << (*itf2).first << "'>" << (*itf2).first << "</option>";
	}
	s << "</select>";
	s << "</td></tr>\n";
}
bool KChain::editModel(KModel *model,std::map<std::string, std::string> &attribute)
{
	if (attribute["revers"] == "1" || attribute["revers"] == "on") {
		model->revers = true;
	} else {
		model->revers = false;
	}
	model->is_or = (attribute["or"] == "1");
	model->editHtml(attribute, true);
	return true;
}
bool KChain::editModel(KModel *model, KUrlValue *urlValue) {
	int index = -1;
	KUrlValue *sub = urlValue->getNextSub(model->getName(), index);
	if (sub == NULL) {
		return false;
	}
	return editModel(model,sub->attribute);
}
void KChain::getAclShortHtml(std::stringstream &s) {
	list<KAcl *>::iterator it;
	if (acls.size() == 0) {
		s << "&nbsp;";
	}
	for (it = acls.begin(); it != acls.end(); it++) {
		s <<  ((*it)->revers ? "!" : "") << (*it)->getName() << ": " << (*it)->getDisplay() ;
		if ((*it)->is_or) {
			s << " [OR NEXT]";
		}
		s << "<br>";
	}
}
void KChain::getMarkShortHtml(std::stringstream &s) {
	list<KMark *>::iterator it;
	if (marks.size() == 0) {
		s << "&nbsp;";
	}
	for (it = marks.begin(); it != marks.end(); it++) {
		s << ((*it)->revers ? "!" : "")	<< (*it)->getName() << ": " <<  (*it)->getDisplay();
		if ((*it)->is_or) {
			s << " [OR NEXT]";
		}
		s << "<br>";
	}
}
bool KChain::findAcl(std::string aclName) {
	list<KAcl *>::iterator it;
	for (it = acls.begin(); it != acls.end(); it++) {
		if (aclName == (*it)->getName()) {
			return true;
		}
	}
	return false;
}
bool KChain::findMark(std::string aclName) {
	list<KMark *>::iterator it;
	for (it = marks.begin(); it != marks.end(); it++) {
		if (aclName == (*it)->getName()) {
			return true;
		}
	}
	return false;
}
bool KChain::edit(KUrlValue *urlValue,KAccess *kaccess,bool editFlag) {
	map<string, string> attribute;
	urlValue->get(attribute);
	this->name = attribute["name"];
	std::string action = attribute["action"];
	if (action.size()>0) {
		kaccess->parseChainAction(action,jumpType,jumpName);
	}else{
		jumpType = atoi(attribute["jump_type"].c_str());
		switch (jumpType) {
		case JUMP_SERVER:
			jumpName = attribute["server"];
			break;
		case JUMP_WBACK:
			jumpName = attribute["wback"];
			break;
		case JUMP_TABLE:
			jumpName = attribute["table"];
			break;
		case JUMP_VHS:
			jumpName = attribute["vhs"];
			break;
		}
	}
	kaccess->setChainAction(jumpType, &jump, jumpName);
	if(!editFlag){
		std::map<std::string,KUrlValue *>::iterator it;
		for(it = urlValue->subs.begin();it!=urlValue->subs.end();it++){
			KUrlValue *sub_uv = (*it).second;
			while (sub_uv) {
				std::string name = sub_uv->attribute["name"];
				if (strncasecmp((*it).first.c_str(),"acl_",4)==0) {
					KAcl *macl = addAcl((*it).first.c_str()+4,name,kaccess);
					if(macl){
						editModel(macl,sub_uv->attribute);
					}
				}else if (strncasecmp((*it).first.c_str(),"mark_",5)==0) {
					KMark *mmark = addMark((*it).first.c_str()+5,name,kaccess);
					if(mmark){
						editModel(mmark,sub_uv->attribute);
					}
				}
				sub_uv = sub_uv->next;
			}
		}
	} else {
		for (std::list<KAcl *>::iterator it = acls.begin(); it != acls.end(); it++) {
			editModel((*it), urlValue);
		}
		for (std::list<KMark *>::iterator it2 = marks.begin(); it2 != marks.end(); it2++) {
			editModel((*it2), urlValue);
		}
	}
	//重新计数
	hit_count = 0;
	return true;
}
KAcl *KChain::newAcl(std::string acl,KAccess *kaccess)
{
	std::map<std::string,KAcl *>::iterator it;
	it = KAccess::aclFactorys[kaccess->type].find(acl);
	if (it!=KAccess::aclFactorys[kaccess->type].end()) {
		KAcl *macl = (*it).second->newInstance();
		if (macl) {
			macl->isGlobal = kaccess->isGlobal();
		}
		return macl;
	}
	return NULL;
}
KMark *KChain::newMark(std::string mark,KAccess *kaccess)
{
	std::map<std::string,KMark *>::iterator it;
	it = KAccess::markFactorys[kaccess->type].find(mark);
	if (it!=KAccess::markFactorys[kaccess->type].end()) {
		KMark *macl = (*it).second->newInstance();
		if (macl) {
			macl->isGlobal = kaccess->isGlobal();
		}
		return macl;
	}
	return NULL;
}
KAcl *KChain::addAcl(std::string acl,std::string name,KAccess *kaccess)
{
	if (name.size()==0) {
		KAcl *a = newAcl(acl,kaccess);
		if (a==NULL) {
			return NULL;
		}
		if (a->addEnd()) {
			acls.push_back(a);
		} else {
			acls.push_front(a);
		}
		return a;
	}
	kfiber_mutex_lock(KAccess::runtimeLock);
	std::stringstream s;
	s << "a" << (int)kaccess->type << (int)kaccess->isGlobal() << name;
	KModel *m = KAccess::getRunTimeModel(s.str());
	if (m==NULL) {
		m = newAcl(acl,kaccess);
		if (m && m->supportRuntime()) {
			m->name = s.str();
			KAccess::addRunTimeModel(m);
			m->addRef();
		}
	} else {
		m->addRef();
	}
	kfiber_mutex_unlock(KAccess::runtimeLock);

	KAcl *a = static_cast<KAcl *>(m);
	if (a) {
		if (a->addEnd()) {
			acls.push_back(a);
		} else {
			acls.push_front(a);
		}
	}
	return a;
}
KMark *KChain::addMark(std::string mark,std::string name,KAccess *kaccess)
{
	if (name.size()==0) {
		KMark *a = newMark(mark,kaccess);
		if (a==NULL) {
			return NULL;
		}
		if (a->addEnd()) {
			marks.push_back(a);
		} else {
			marks.push_front(a);
		}
		return a;
	}
	kfiber_mutex_lock(KAccess::runtimeLock);
	std::stringstream s;
	s << "m" << (int)kaccess->type << (int)kaccess->isGlobal() << name;
	KModel *m = KAccess::getRunTimeModel(s.str());
	if (m==NULL) {
		m = newMark(mark,kaccess);
		if (m && m->supportRuntime()) {
			m->name = s.str();
			KAccess::addRunTimeModel(m);
		}
	} else {
		m->addRef();
	}
	kfiber_mutex_unlock(KAccess::runtimeLock);
	KMark *a = static_cast<KMark *>(m);
	if (a) {
		if (a->addEnd()) {
			marks.push_back(a);
		} else {
			marks.push_front(a);
		}
	}
	return a;
}
bool KChain::downAcl(std::string id) {
	std::list<KAcl *>::iterator it;
	KAcl *model;
	int index = atoi(id.c_str());
	for (it = acls.begin(); it != acls.end(); it++) {
		if (index == 0) {
			model = (*it);
			it = acls.erase(it);
			if (it==acls.end()) {
				acls.push_front(model);
			} else {
				it++;
				acls.insert(it,model);
			}
			break;
		}
		index--;
	}
	return true;
}
bool KChain::downMark(std::string id) {
	std::list<KMark *>::iterator it;
	KMark *model;
	int index = atoi(id.c_str());
	for (it = marks.begin(); it != marks.end(); it++) {
		if (index == 0) {
			model = (*it);
			it = marks.erase(it);
			if (it==marks.end()) {
				marks.push_front(model);
			} else {
				it++;
				marks.insert(it,model);
			}
			break;
		}
		index--;
	}
	return true;
}
bool KChain::delAcl(std::string acl) {
	std::list<KAcl *>::iterator it;
	int index = atoi(acl.c_str());
	for (it = acls.begin(); it != acls.end(); it++) {
		if (index == 0) {
			(*it)->release();
			acls.erase(it);
			return true;
		}
		index--;
	}
	return false;
}
bool KChain::delMark(std::string mark) {
	std::list<KMark *>::iterator it;
	int index = atoi(mark.c_str());
	for (it = marks.begin(); it != marks.end(); it++) {
		if (index == 0) {
			(*it)->release();
			marks.erase(it);
			return true;
		}
		index--;
	}
	return false;
}
bool KChain::startElement(KXmlContext *context, std::map<std::string,
		std::string> &attribute,KAccess *kaccess) {
	string qName = context->qName;
	int elementType = 0;
	if(context->qName == CHAIN_CONTEXT){
		std::string action = attribute["action"];
		kaccess->parseChainAction(attribute["action"], jumpType,jumpName);
		name = attribute["name"];
		return true;
	}
	if (strncasecmp(qName.c_str(), "acl_", 4) == 0) {
		qName = qName.substr(4);
		elementType = MODEL_ACL;
	} else if (strncasecmp(qName.c_str(), "mark_", 5) == 0) {
		qName = qName.substr(5);
		elementType = MODEL_MARK;
	}
	if (context->getParentName() == ACL_CONTEXT || elementType == MODEL_ACL) {
		std::map<std::string,KAcl *>::iterator it;
		curacl = addAcl(qName,attribute["name"],kaccess);
		if (curacl) {
			curacl->revers = (attribute["revers"] == "1");
			curacl->is_or = (attribute["or"] == "1");
			curacl->startElement(context, attribute);
		} else {
			fprintf(stderr, "cann't load acl model[%s]\n", qName.c_str());
		}
		return true;
	}
	if (context->getParentName() == MARK_CONTEXT || elementType == MODEL_MARK) {
		std::map<std::string,KMark *>::iterator it;
		curmark = addMark(qName,attribute["name"],kaccess);
		if (curmark) {
			curmark->revers = (attribute["revers"] == "1");
			curmark->is_or = (attribute["or"] == "1");
			curmark->startElement(context, attribute);
		} else {
			fprintf(stderr, "cann't load mark model[%s]\n", qName.c_str());
		}
		return true;
	}
	if (curacl) {
		return curacl->startElement(context, attribute);
	}

	if (curmark) {
		return curmark->startElement(context, attribute);
	}
	return true;
}
bool KChain::startCharacter(KXmlContext *context, char *character, int len) {
	if (curacl) {
		return curacl->startCharacter(context, character, len);
	}
	if (curmark) {
		return curmark->startCharacter(context, character, len);
	}
	return true;
}
bool KChain::endElement(KXmlContext *context) {
	if (context->getParentName() == ACL_CONTEXT || 
		strncasecmp(context->qName.c_str(), "acl_", 4) == 0) {
		if (curacl) {
			curacl->endElement(context);
			curacl = NULL;
		}
		return true;
	}
	if (context->getParentName() == MARK_CONTEXT || 
		strncasecmp(context->qName.c_str(), "mark_", 5) == 0) {
		if (curmark) {
			curmark->endElement(context);
			curmark = NULL;
		}
		return true;
	}
	if (curacl) {
		return curacl->endElement(context);
	}
	if (curmark) {
		return curmark->endElement(context);
	}
	return true;
}
void KChain::buildXML(std::stringstream &s,int flag) {
	KAccess::buildChainAction(jumpType, jump, s);
	if (name.size() > 0) {
		s << " name='" << name << "'";
	}
	s << ">\n";
	if (KBIT_TEST(flag,CHAIN_XML_SHORT)) {
		return;
	}	
	for (std::list<KAcl *>::iterator it = acls.begin(); it != acls.end(); it++) {
		s << "\t\t\t\t<acl_";
		s << (*it)->getName();
		if ((*it)->revers) {
			s << " revers='1'";
		}
		if ((*it)->is_or) {
			s << " or='1'";
		}
		if ((*it)->name.size()>3) {
			s << " name='" << (*it)->name.substr(3) << "'";
		}
		s << " ";
		(*it)->buildXML(s,flag);
		s << "</acl_" << (*it)->getName() << ">\n";
	}
	for (std::list<KMark *>::iterator it2 = marks.begin(); it2 != marks.end(); it2++) {
		s << "\t\t\t\t<mark_";
		s << (*it2)->getName() << " ";
		if ((*it2)->revers) {
			s << " revers='1'";
		}
		if ((*it2)->is_or) {
			s << " or='1'";
		}
		if ((*it2)->name.size()>3) {
			s << " name='" << (*it2)->name.substr(3) << "'";
		}
		s << " ";
		(*it2)->buildXML(s,flag);
		s << "</mark_" << (*it2)->getName() << ">\n";
	}
}
