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
#include "KModelManager.h"
#include "KTable.h"
#include "KHttpLib.h"
#include<list>
#include "kmalloc.h"
using namespace std;

void parse_module_child_config(KModel* m, const KMap<khttpd::KXmlKey, khttpd::KXmlNode>& childs) {
	kconfig::KXmlChanged changed = { 0 };
	kgl_config_diff diff{ 0 };
	changed.type = kconfig::EvNew;
	changed.diff = &diff;
	for (auto node : childs) {
		changed.new_xml = node;
		changed.diff->new_to = changed.new_xml->get_body_count();
		m->parse_child(&changed);
	}
}
KChain::KChain() {
	hit_count = 0;
	jump_type = JUMP_CONTINUE;
}
KChain::~KChain() {
	clear();
}
void KChain::clear() {
	jump = nullptr;
	for (auto it = acls.begin(); it != acls.end(); ++it) {
		(*it)->release();
	}
	acls.clear();
	for (auto it = marks.begin(); it != marks.end(); ++it) {
		(*it)->release();
	}
	marks.clear();
}
bool KChain::match(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) {
	bool result = true;
	bool last_or = false;
	//OR NEXT
	for (auto it = acls.begin(); it != acls.end(); ++it) {
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
	for (auto it = marks.begin(); it != marks.end(); ++it) {
		if (result && last_or) {
			last_or = (*it)->is_or;
			continue;
		}
		result = ((*it)->process(rq, obj, fo) != (*it)->revers);
		if (!result && !last_or) {
			break;
		}
		if (fo) {
			return true;
		}
	}
	if (result) {
		hit_count++;
	}
	return result;
}
void KChain::get_edit_html(KWStream& s, u_short accessType) {
	int index = 0;
	for (auto it = acls.begin(); it != acls.end(); ++it) {
		getModelHtml((*it), s, 0, index);
		index++;
	}
	s << "<tr><td>" << klang["available_acls"] << "</td><td>";
	s
		<< "<select onChange='addmodel(this.options[this.options.selectedIndex].value,0)'><option value=''>";
	s << klang["select_acls"] << "</option>";
	for (auto itf = KAccess::acl_factorys[accessType].begin(); itf
		!= KAccess::acl_factorys[accessType].end(); itf++) {
		s << "<option value='" << (*itf).first << "'>" << (*itf).first
			<< "</option>";
	}
	s << "</select>\n";
	//s << "<table><div id='acls' class='acls'>test</div></table>\n";
	s << "</td></tr>\n";
	index = 0;
	for (auto it = marks.begin(); it != marks.end(); ++it) {
		getModelHtml((*it), s, 1, index);
		index++;
	}
	//ʾmark
	s << "<tr><td>" << klang["available_marks"] << "</td><td>";
	s << "<select onChange='";
	s << "addmodel(this.options[this.options.selectedIndex].value,1)'>";
	s << "<option value=''>" << klang["select_marks"] << "</option>";
	for (auto itf2 = KAccess::mark_factorys[accessType].begin(); itf2
		!= KAccess::mark_factorys[accessType].end(); itf2++) {
		s << "<option value='" << (*itf2).first << "'>" << (*itf2).first << "</option>";
	}
	s << "</select>";
	s << "</td></tr>\n";
}
void KChain::getModelHtml(KModel* model, KWStream& s, int type, int index) {

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
	model->get_html(model, s);
	s << "<input type=hidden name='end_sub_form' value='1'></td></tr>\n";
}
void KChain::get_acl_short_html(KWStream& s) {
	if (acls.empty()) {
		s << "&nbsp;";
	}
	for (auto it = acls.begin(); it != acls.end(); ++it) {
		s << ((*it)->revers ? "!" : "") << (*it)->getName() << ": ";
		(*it)->get_display(s);
		if ((*it)->is_or) {
			s << " [OR NEXT]";
		}
		s << "<br>";
	}
}
void KChain::get_mark_short_html(KWStream& s) {
	if (marks.empty()) {
		s << "&nbsp;";
	}
	for (auto it = marks.begin(); it != marks.end(); ++it) {
		s << ((*it)->revers ? "!" : "") << (*it)->getName() << ": ";
		(*it)->get_display(s);
		if ((*it)->is_or) {
			s << " [OR NEXT]";
		}
		s << "<br>";
	}
}
void KChain::parse_config(KAccess* access, const khttpd::KXmlNodeBody* xml) {
	assert(acls.empty());
	assert(marks.empty());
	KString jumpName;
	if (!access->parseChainAction(xml->attributes["action"], jump_type, jumpName)) {
		throw KXmlException("chain action error");
	}
	access->setChainAction(jump_type, jump, jumpName);
	for (auto node : xml->childs) {
		auto model_name = node->get_tag();
		bool is_acl = false;
		if (strncasecmp(model_name.c_str(), "acl_", 4) == 0) {
			is_acl = true;
			model_name = model_name.substr(4);
		} else if (strncasecmp(model_name.c_str(), "mark_", 5) == 0) {
			is_acl = false;
			model_name = model_name.substr(5);
		} else if (node->is_tag(_KS("acl"))) {
			for (auto&& body : node->body) {
				auto model_name = body->attributes["module"];
				if (model_name.empty()) {
					auto ref = body->attributes["ref"];
					if (ref) {
						auto m = access->get_named_acl(ref);
						if (m) {
							acls.push_back(m.release());
						}
					}
					continue;
				}
				auto m = access->new_acl(model_name, body);
				if (!m) {
					continue;
				}
				parse_module_child_config(m.get(), body->childs);
				acls.push_back(m.release());
			}
			continue;
		} else if (node->is_tag(_KS("mark"))) {
			for (auto&& body : node->body) {
				auto model_name = body->attributes["module"];
				if (model_name.empty()) {
					auto ref = body->attributes["ref"];
					if (ref) {
						auto m = access->get_named_mark(ref);
						if (m) {
							marks.push_back(m.release());
						}
					}
					continue;
				}
				auto m = access->new_mark(model_name, body);
				if (!m) {
					continue;
				}
				parse_module_child_config(m.get(), body->childs);
				marks.push_back(m.release());
			}
			continue;
		} else {
			klog(KLOG_ERR, "unknow qname [%s] in chain\n", model_name.c_str());
			continue;
		}
		for (auto&& body : node->body) {
			if (is_acl) {
				auto m = access->new_acl(model_name, body);
				if (!m) {
					continue;
				}
				parse_module_child_config(m.get(), body->childs);
				acls.push_back(m.release());
			} else {
				auto m = access->new_mark(model_name, body);
				if (!m) {
					continue;
				}
				parse_module_child_config(m.get(), body->childs);
				marks.push_back(m.release());
			}
		}
	}
}