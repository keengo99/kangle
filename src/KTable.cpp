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
#include "KTable.h"
#include "KChain.h"
#include "KModelManager.h"
#include "kmalloc.h"
#include "time_utils.h"

void KTable::clear() {	
	chains.clear();
}
KSafeChain KTable::parse_chain(const khttpd::KXmlNodeBody* xml) {
	KSafeChain chain(new KChain());
	chain->parse_config(access, xml);
	return chain;
}
void KTable::on_file_event(std::vector<KSafeChain>& chain, kconfig::KConfigEvent* ev) {
	uint32_t old_count = ev->diff->old_to - ev->diff->from;
	//new
	for (uint32_t index = ev->diff->from; index < ev->diff->new_to; ++index) {
		chain.insert(chain.begin() + index + old_count, parse_chain(ev->new_xml->get_body(index)));
	}
	//remove
	for (uint32_t index = ev->diff->from; index < ev->diff->old_to; ++index) {
		chain.erase(chain.begin()+ ev->diff->from);
	}
	chain.shrink_to_fit();
}
bool KTable::on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (!ev->get_xml()->is_tag(_KS("chain"))) {
		return false;
	}
	auto locker = access->write_lock();	
	auto result = chains.emplace(std::piecewise_construct,std::forward_as_tuple(ev->file->get_index(), ev->file->get_name()), std::tuple<>());
	auto it = result.first;
	on_file_event((*it).second, ev);
	if ((*it).second.size() == 0) {
		chains.erase(it);
	}
	return true;
}
bool KTable::parse_config(KAccess* access, const khttpd::KXmlNodeBody* xml) {
	return true;
}
kgl_jump_type KTable::match(KHttpRequest* rq, KHttpObject* obj, unsigned& checked_table, KJump** jump, KSafeSource &fo) {
	KTable *m_table = NULL;
	for (auto&& chain_file : chains) {
		for (auto&& chain : chain_file.second) {
			if (!chain) {
				continue;
			}
			bool result = chain->match(rq, obj, fo);
			if (fo) {
				return chain->jumpType;
			}
			if (!result) {
				continue;
			}
			switch (chain->jumpType) {
			case JUMP_CONTINUE:
				break;
			case JUMP_TABLE:
			{
				m_table = static_cast<KTable*>(chain->jump);
				if (!m_table) {
					return JUMP_DENY;
				}
				if (checked_table++ > 32) {
					//jump tableÌ«¶à
					return JUMP_DENY;
				}
				int jump_type = m_table->match(rq, obj, checked_table, jump, fo);
				if (jump_type != JUMP_CONTINUE) {
					return jump_type;
				}
				break;
			}
			default:
				*jump = chain->jump;
				return chain->jumpType;
			}
		}
	}
	return JUMP_CONTINUE;
}
void KTable::htmlTable(KWStream &s,const char *vh,u_short accessType) {
	s << name << "," << LANG_REFS << get_ref();
#if 0
	s << "[<a href=\"javascript:if(confirm('" << LANG_CONFIRM_DELETE << name
			<< "')){ window.location='tabledel?vh=" << vh << "&access_type=" << accessType
			<< "&table_name=" << name << "';}\">" << LANG_DELETE << "</a>]";
	s << "[<a href=\"javascript:if(confirm('" << LANG_CONFIRM_EMPTY << name
			<< "')){ window.location='tableempty?vh=" << vh << "&access_type=" << accessType
			<< "&table_name=" << name << "';}\">" << LANG_EMPTY << "</a>]";
	s << "[<a href=\"javascript:tablerename(" << accessType << ",'" << name << "');\">"
			<< LANG_RENAME << "</a>]";
#endif
	s << "<table border=1 cellspacing=0 width=100%><tr>";
#if 0
	<td> << LANG_OPERATOR
		<< "</td><td>" << klang["id"] << "</td>";
#endif
	s << "<td>" << LANG_ACTION << "</td><td>acl</td><td>mark</td><td>";
	s << LANG_HIT_COUNT << "</td></tr>\n";
	int j = 0;
	int id = 0;
	for (auto&& chain_file : chains) {
		for (auto&& chain : chain_file.second) {
			if (!chain) {
				continue;
			}
			s << "<tr><td>";
#if 0
			s << "[< a href = \"javascript:if(confirm('Are you sure to delete the chain";
			s << "')){ window.location='delchain?vh=" << vh << "&access_type=" << accessType
				<< "&id=" << j << "&table_name=" << name << "';}\">"
				<< LANG_DELETE << "</a>][<a href='editchainform?vh=" << vh << "&access_type="
				<< accessType << "&id=" << j << "&table_name=" << name
				<< "'>" << LANG_EDIT << "</a>][<a href='addchain?vh=" << vh << "&access_type="
				<< accessType << "&id=" << j << "&table_name=" << name
				<< "'>" << LANG_INSERT << "</a>]"
				<< "[<a href='downchain?vh=" << vh << "&access_type="
				<< accessType << "&id=" << j << "&table_name=" << name
				<< "'>down</a>]"
				s << "</td><td>";
#endif
			switch (chain->jumpType) {
			case JUMP_DENY:
				s << LANG_DENY;
				break;
			case JUMP_ALLOW:
				s << LANG_ALLOW;
				break;
			case JUMP_CONTINUE:
				s << klang["LANG_CONTINUE"];
				break;
			case JUMP_TABLE:
				s << LANG_TABLE;
				break;
			case JUMP_SERVER:
				s << klang["server"];
				break;
			case JUMP_WBACK:
				s << LANG_WRITE_BACK;
				break;
			case JUMP_FCGI:
				s << "fastcgi";
				break;
			case JUMP_PROXY:
				s << klang["proxy"];
				break;
			case JUMP_RETURN:
				s << klang["return"];
				break;
			}
			if (chain->jump) {
				s << ":" << chain->jump->name;
			}
			s << "</td><td>";
			chain->get_acl_short_html(s);
			s << "</td><td>";
			chain->get_mark_short_html(s);
			s << "</td>";
			s << "<td>" << chain->hit_count << "</td>";
			s << "</tr>\n";
			j++;
		}
	}
	s << "</table>";
#if 0
	s << "[<a href='addchain?vh=" << vh << "&access_type=" << accessType
			<< "&add=1&id=-1&table_name=" << name << "'>" << LANG_INSERT
			<< "</a>]<br><br>";
#endif
}
KTable::KTable(KAccess *access,const KString&name) : KJump(name) {
	this->access = access;
}
KTable::~KTable() {
	clear();
}
