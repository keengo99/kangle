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
void parse_module_config(KModel* m, const khttpd::KXmlNodeBody* xml) {
	m->revers = (xml->attributes["revers"] == "1");
	m->is_or = (xml->attributes["or"] == "1");
	m->parse_config(xml);
	kconfig::KXmlChanged changed = { 0 };
	kgl_config_diff diff{ 0 };
	changed.type = kconfig::EvNew;
	changed.diff = &diff;
	for (auto node : xml->childs) {
		changed.new_xml = node;
		changed.diff->new_to = changed.new_xml->get_body_count();
		m->parse_child(&changed);
	}
}
KChain::KChain() {
	hit_count = 0;
	jump = nullptr;
	jumpType = JUMP_CONTINUE;	
}
KChain::~KChain() {
	clear();
}
void KChain::clear()
{
	if (jump) {
		jump->release();
		jump = nullptr;
	}
	acls.clear();
	marks.clear();
}
bool KChain::match(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) {
	bool result = true;
	bool last_or = false;
	//OR NEXT
	for (auto it = acls.begin(); it != acls.end(); it++) {
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
	for (auto it2 = marks.begin(); it2 != marks.end(); it2++) {
		if (result && last_or) {
			last_or = (*it2)->is_or;
			continue;
		}
		result = ((*it2)->mark(rq, obj, fo) != (*it2)->revers);
		if (fo && *fo) {
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
void KChain::getModelHtml(KModel *model, std::stringstream &s, int type, int index) {

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
void KChain::getAclShortHtml(std::stringstream &s) {
	if (acls.size() == 0) {
		s << "&nbsp;";
	}
	for (auto it = acls.begin(); it != acls.end(); ++it) {
		s <<  ((*it)->revers ? "!" : "") << (*it)->getName() << ": " << (*it)->getDisplay() ;
		if ((*it)->is_or) {
			s << " [OR NEXT]";
		}
		s << "<br>";
	}
}
void KChain::getMarkShortHtml(std::stringstream &s) {
	if (marks.size() == 0) {
		s << "&nbsp;";
	}
	for (auto it = marks.begin(); it != marks.end(); it++) {
		s << ((*it)->revers ? "!" : "")	<< (*it)->getName() << ": " <<  (*it)->getDisplay();
		if ((*it)->is_or) {
			s << " [OR NEXT]";
		}
		s << "<br>";
	}
}
KSafeAcl KChain::new_acl(const std::string& name, KAccess* kaccess) {
	auto it = KAccess::aclFactorys[kaccess->type].find(name);
	if (it == KAccess::aclFactorys[kaccess->type].end()) {
		return nullptr;
	}
	KSafeAcl m((*it).second->new_instance());
	if (m) {
		m->isGlobal = kaccess->isGlobal();
	}
	return m;
}
KSafeMark KChain::new_mark(const std::string& name, KAccess* kaccess) {
	auto it = KAccess::markFactorys[kaccess->type].find(name);
	if (it == KAccess::markFactorys[kaccess->type].end()) {
		return nullptr;
	}
	KSafeMark m((*it).second->new_instance());
	if (m) {
		m->isGlobal = kaccess->isGlobal();
	}
	return m;
}
void KChain::parse_config(KAccess *access,const khttpd::KXmlNodeBody* xml) {
	assert(acls.empty());
	assert(marks.empty());
	std::string jumpName;
	if (!access->parseChainAction(xml->attributes["action"], jumpType, jumpName)) {
		throw KXmlException("chain action error");
	}
	access->setChainAction(jumpType, &jump, jumpName);
	for (auto node : xml->childs) {
		auto qName = node->get_tag();
		bool is_acl;
		if (strncasecmp(qName.c_str(), "acl_", 4) == 0) {
			qName = qName.substr(4);
			is_acl = true;
		} else if (strncasecmp(qName.c_str(), "mark_", 5) == 0) {
			qName = qName.substr(5);
			is_acl = false;
		} else {
			klog(KLOG_ERR, "unknow qname [%s] in chain\n", qName.c_str());
			continue;
		}
		for (uint32_t index = 0;; ++index) {
			auto body = node->get_body(index);
			if (!body) {
				break;
			}
			if (is_acl) {
				auto acl = new_acl(qName, access);
				if (!acl) {
					continue;
				}
				parse_module_config(acl.get(), body);
				acls.push_back(std::move(acl));
			} else {
				auto mark = new_mark(qName, access);
				if (!mark) {
					continue;
				}
				parse_module_config(mark.get(), body);
				marks.push_back(std::move(mark));
			}
		}
	}	
}