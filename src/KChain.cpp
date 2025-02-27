﻿/*
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
	/*
	for (auto it = acls.begin(); it != acls.end(); ++it) {
		(*it)->release();
	}
	acls.clear();
	for (auto it = marks.begin(); it != marks.end(); ++it) {
		(*it)->release();
	}
	marks.clear();
	*/
}
void KChain::get_edit_html(kgl::serializable* s) {
	for (auto it = acls.begin(); it != acls.end(); ++it) {
		auto acl = s->add_obj_array("acl");
		if (!acl) {
			return;
		}
	}
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

void KChain::dump(kgl::serializable* s,bool is_short) {
	KStringBuf action;
	switch (jump_type) {
	case JUMP_DENY:
		s->add("action", "deny");
		break;
	case JUMP_ALLOW:
		s->add("action", "allow");
		break;
	case JUMP_CONTINUE:
		s->add("action", "continue");
		break;
	case JUMP_TABLE:
		s->add("action", "table");
		break;
	case JUMP_SERVER:
		s->add("action", "server");
		break;
	case JUMP_WBACK:
		s->add("action", "wback");
		break;
	case JUMP_PROXY:
		s->add("action", "proxy");
		break;
	case JUMP_RETURN:
		s->add("action", "return");
		break;
	case JUMP_DROP:
		s->add("action", "drop");
		break;
	}
	if (jump) {
		s->add("jump", jump->name);
	}
	s->add("hit", hit_count);
	for (auto it = acls.begin(); it != acls.end(); ++it) {
		auto m = s->add_obj_array("acl");
		if (!m) {
			return;
		}
		(*it).dump(m, is_short);
	}
	for (auto it = marks.begin(); it != marks.end(); ++it) {
		auto m = s->add_obj_array("mark");
		if (!m) {
			return;
		}		
		(*it).dump(m, is_short);
	}
}
void KChain::get_acl_short_html(KWStream& s) {
	if (acls.empty()) {
		s << "&nbsp;";
	}
	for (auto it = acls.begin(); it != acls.end(); ++it) {
		s << ((*it).revers ? "!" : "") << (*it).m->get_module() << ": ";
		(*it).m->get_display(s);
		if ((*it).is_or) {
			s << " [OR]";
		}
		s << "<br>";
	}
}
void KChain::get_mark_short_html(KWStream& s) {
	if (marks.empty()) {
		s << "&nbsp;";
	}
	for (auto it = marks.begin(); it != marks.end(); ++it) {
		s << ((*it).revers ? "!" : "") << (*it).m->get_module() << ": ";
		(*it).m->get_display(s);
		if ((*it).is_or) {
			s << " [OR]";
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
		if (node->is_tag(_KS("acl"))) {
			for (auto&& body : node->body) {
				auto ref = body->attributes["ref"];
				if (ref) {
					auto m = access->get_named_acl(ref);
					if (m) {
						m.parse_config(body);
						acls.push_back(m);
					}
				} else {
					auto model_name = body->attributes["module"];
					if (!model_name) {
						continue;
					}
					auto m = access->new_acl(model_name, body);
					if (!m) {
						continue;
					}
					parse_module_child_config(m.m, body->childs);
					acls.push_back(m);
				}
			}
		} else if (node->is_tag(_KS("mark"))) {
			for (auto&& body : node->body) {
				auto ref = body->attributes["ref"];
				if (ref) {
					auto m = access->get_named_mark(ref);
					if (m) {
						m.parse_config(body);
						marks.push_back(m);
					}
				} else {
					auto model_name = body->attributes["module"];
					if (!model_name) {
						continue;
					}
					auto m = access->new_mark(model_name, body);
					if (!m) {
						continue;
					}
					parse_module_child_config(m.m, body->childs);
					marks.push_back(m);
				}
			}
		} else {
			klog(KLOG_ERR, "unknow qname [%s] in chain\n", node->get_tag().c_str());
		}
	}
	acls.shrink_to_fit();
	marks.shrink_to_fit();
}
khttpd::KSafeXmlNode KChain::to_xml(KUrlValue& uv) {
	auto xml = kconfig::new_xml("chain"_CS);
	KAccess::build_action_attribute(xml->attributes(), uv);
	for (auto it = uv.subs.begin(); it != uv.subs.end(); ++it) {
		if (strncmp((*it).first.c_str(), _KS("acl_")) == 0) {
			auto acl = (*it).second->to_xml(_KS("acl"));
			acl->attributes().emplace("module"_CS, (*it).first.substr(4));
			xml->insert(acl.get(), khttpd::last_pos);
		} else if (strncmp((*it).first.c_str(), _KS("mark_")) == 0) {
			auto mark = (*it).second->to_xml(_KS("mark"));
			mark->attributes().emplace("module"_CS, (*it).first.substr(5));
			xml->insert(mark.get(), khttpd::last_pos);
		}
	}
	return xml;
}
