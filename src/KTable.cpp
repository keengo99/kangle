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
using namespace std;

bool KTable::startElement(KXmlContext *context, std::map<std::string,
		std::string> &attribute,KAccess *kaccess) {

	if (context->getParentName() == TABLE_CONTEXT && context->qName	== CHAIN_CONTEXT) {
		if (curChain) {
			delete curChain;
		}
		curChain = new KChain();
	}
	if (curChain) {
		return curChain->startElement(context, attribute,kaccess);
	}
	return false;
}
void KTable::empty() {
	while (head) {
		end = head->next;
		delete head;
		head = end;
	}
	chain_map.clear();
}
void KTable::pushChain(KChain *newChain)
{
	newChain->next = NULL;
	newChain->prev = end;
	if (end) {
		end->next = newChain;
	} else {
		head = newChain;
	}
	end = newChain;
}
int KTable::insertChain(int index, KChain *newChain) {
	if (newChain->name.size() > 0) {
		std::map<std::string, KChain *>::iterator it;
		it = chain_map.find(newChain->name);
		if (it != chain_map.end()) {
			delete newChain;
			return -1;
		}
		chain_map.insert(std::pair<std::string, KChain *>(newChain->name, newChain));
	}
	if (index==-1) {
		pushChain(newChain);
		return index;
	}
	KChain *point = findChain(index);
	if (point==NULL) {
		pushChain(newChain);
		return index;
	}
	newChain->next = point;
	newChain->prev = point->prev;
	if (point->prev) {
		point->prev->next = newChain;
	}
	point->prev = newChain;
	if (head==point) {
		head = newChain;
	}
	return index;
}
void KTable::removeChain(KChain *chain)
{
	if (chain->prev) {
		chain->prev->next = chain->next;
	}
	if (chain->next) {
		chain->next->prev = chain->prev;
	}
	if (head==chain) {
		head = head->next;
	}
	if (end==chain) {
		end = end->prev;
	}
}
bool KTable::delChain(std::string name) {
	std::map<std::string,KChain *>::iterator it;
	it = chain_map.find(name);
	if (it==chain_map.end()) {
		return false;
	}
	KChain *chain = (*it).second;
	chain_map.erase(it);
	removeChain(chain);
	delete chain;
	return true;
 }
 bool KTable::editChain(std::string name, KUrlValue *urlValue,KAccess *kaccess) {
	 std::map<std::string,KChain *>::iterator it;
	 it = chain_map.find(name);
	 if (it==chain_map.end()) {
		KChain *chain = new KChain();
		chain->name = name;
		chain->edit(urlValue,kaccess,false);
		insertChain(-1,chain);
		chainChangeName("",chain);
		return true;
	 }
	 KChain *chain = (*it).second;
	 chain->clear();
	 chain->edit(urlValue,kaccess,false);
	 chainChangeName(name,chain);
	 return true;
}

bool KTable::match(KHttpRequest *rq, KHttpObject *obj, int &jumpType,
		KJump **jumpTable, unsigned &checked_table,
		const char **hitTable, int *chainId) {
	vector<KChain *>::iterator it;
	KTable *m_table = NULL;
	KChain *matchChain = head;
	int id = 0;
	while (matchChain) {
		id++;
		if (matchChain->match(rq, obj, jumpType, jumpTable)) {
			switch (jumpType) {
			case JUMP_CONTINUE:
				matchChain = matchChain->next;
				continue;
			case JUMP_TABLE:
				m_table = (KTable *) (*jumpTable);
				if(m_table){
					assert(m_table);
					if(checked_table++ > 32){
						jumpType = JUMP_DENY;
						//jump tableÌ«¶à
						return true;
					}
					if (m_table->match(rq,
						obj,
						jumpType,
						jumpTable,
						checked_table,
						hitTable,
						chainId) &&
						jumpType != JUMP_RETURN) {
						return true;
					}
				}
				matchChain = matchChain->next;
				continue;
			default:
				*hitTable = name.c_str();
				*chainId = id;
				return true;
			}
		}
		matchChain = matchChain->next;
	}
	return false;
}
bool KTable::startCharacter(KXmlContext *context, char *character, int len) {
	if (curChain) {
		return curChain->startCharacter(context, character, len);
	}
	return false;
}
bool KTable::endElement(KXmlContext *context) {

	if (context->getParentName() == TABLE_CONTEXT && context->qName
			== CHAIN_CONTEXT) {
		if (curChain) {
			insertChain(-1, curChain);
			curChain = NULL;
		}
		return true;
	}
	if (curChain) {
		return curChain->endElement(context);
	}
	return false;
}
void KTable::htmlTable(std::stringstream &s,const char *vh,u_short accessType) {
	s << name << "," << LANG_REFS << refs;
	s << "[<a href=\"javascript:if(confirm('" << LANG_CONFIRM_DELETE << name
			<< "')){ window.location='tabledel?vh=" << vh << "&access_type=" << accessType
			<< "&table_name=" << name << "';}\">" << LANG_DELETE << "</a>]";
	s << "[<a href=\"javascript:if(confirm('" << LANG_CONFIRM_EMPTY << name
			<< "')){ window.location='tableempty?vh=" << vh << "&access_type=" << accessType
			<< "&table_name=" << name << "';}\">" << LANG_EMPTY << "</a>]";
	s << "[<a href=\"javascript:tablerename(" << accessType << ",'" << name << "');\">"
			<< LANG_RENAME << "</a>]";
	s << "<table border=1 cellspacing=0 width=100%><tr><td>" << LANG_OPERATOR
			<< "</td><td>" << klang["id"] << "</td><td>" << LANG_ACTION
			<< "</td><td>" << klang["acl"] << "</td><td>" << klang["mark"]
			<< "</td><td>";
	s << LANG_HIT_COUNT << "</td></tr>\n";
	int j = 0;
	int id = 0;
	KChain *chain = head;
	while (chain) {
		s
				<< "<tr><td>[<a href=\"javascript:if(confirm('Are you sure to delete the chain";
		s << "')){ window.location='delchain?vh=" << vh << "&access_type=" << accessType
				<< "&id=" << j << "&table_name=" << name << "';}\">"
				<< LANG_DELETE << "</a>][<a href='editchainform?vh=" << vh << "&access_type="
				<< accessType << "&id=" << j << "&table_name=" << name
				<< "'>" << LANG_EDIT << "</a>][<a href='addchain?vh=" << vh << "&access_type="
				<< accessType << "&id=" << j << "&table_name=" << name
				<< "'>" << LANG_INSERT << "</a>]"
				<< "[<a href='downchain?vh=" << vh << "&access_type="
				<< accessType << "&id=" << j << "&table_name=" << name
				<< "'>down</a>]</td><td><div>" << id++ << " " << chain->name << "</div></td><td>";
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
		case JUMP_VHS:
			s << klang["vhs"];
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
		chain->getAclShortHtml(s);
		s << "</td><td>";
		chain->getMarkShortHtml(s);
		s << "</td>";
		s << "<td>" << chain->hit_count << "</td>";
		s << "</tr>\n";
		j++;
		chain = chain->next;
	}
	s << "</table>";
	s << "[<a href='addchain?vh=" << vh << "&access_type=" << accessType
			<< "&add=1&id=-1&table_name=" << name << "'>" << LANG_INSERT
			<< "</a>]<br><br>";

}
bool KTable::buildXML(const char *chain_name,std::stringstream &s,int flag)
{
	if (chain_name==NULL || *chain_name=='\0') {
		buildXML(s,flag);
		return true;
	}
	KChain *m_chain = findChain(chain_name);
	if (m_chain==NULL) {
		return false;
	}
	s << "\t\t<table name='" << this->name << "'>\n";
	s << "\t\t\t<chain ";//id='" << index++ << "'";
	m_chain->buildXML(s,flag);
	s << "\t\t\t</chain>\n";
	s << "\t\t</table>\n";
	return true;
}
void KTable::buildXML(std::stringstream &s,int flag) {
	std::stringstream c;
	KChain *chain = head;
	while (chain) {
		if (KBIT_TEST(flag,CHAIN_SKIP_EXT) && chain->ext) {
			chain = chain->next;
			continue;
		}
		c << "\t\t\t<chain ";
		chain->buildXML(c,flag);
		c << "\t\t\t</chain>\n";
		chain = chain->next;
	}
	if (KBIT_TEST(flag,CHAIN_SKIP_EXT) && ext && c.str().size()==0) {
		return;
	}
	s << "\t\t<table name='" << this->name << "'>\n";
	s << c.str();
	s << "\t\t</table>\n";
	return;
}
KTable::KTable() {
	curChain = NULL;
	ext = cur_config_ext;
	head = NULL;
	end = NULL;
}
KTable::~KTable() {
	empty();
}
std::string KTable::addChainForm(KChain *chain,u_short accessType) {
	stringstream s;
	if (chain) {
		chain->getEditHtml(s,accessType);
	}
	return s.str();
}
bool KTable::downChain(int index) {
	KChain *chain = findChain(index);
	if (chain==NULL) {
		return false;
	}
	KChain *next = chain->next;
	removeChain(chain);
	if (next==NULL) {
		chain->next = head;
		chain->prev = NULL;
		if (head) {
			head->prev = chain;
		} else {
			end = chain;
		}
		head = chain;
	} else {
		if (next->next==NULL) {
			pushChain(chain);
		} else {			
			chain->next = next->next;
			chain->prev = next;
			next->next->prev = chain;
			next->next = chain;
		}
	}
	return true;
}
bool KTable::delChain(int index) {
	KChain *chain = findChain(index);
	if (chain==NULL) {
		return false;
	}
	if (chain->name.size()>0) {
		std::map<std::string,KChain *>::iterator it;
		it = chain_map.find(chain->name);
		if (it!=chain_map.end() && chain==(*it).second) {
			chain_map.erase(it);
		}
	}
	removeChain(chain);
	delete chain;
	return true;
}
bool KTable::editChain(int index, KUrlValue *urlValue,KAccess *kaccess) {
	KChain *chain = findChain(index);
	if (chain==NULL) {
		return false;
	}
	std::string name = chain->name;
	std::string new_name = urlValue->get("name");
	if (new_name.size() > 0 && new_name!=name) {
		if (findChain(new_name.c_str())) {
			return false;
		}
	}
	bool result = chain->edit(urlValue,kaccess,true);
	chainChangeName(name,chain);
	return result;
}
bool KTable::addAcl(int index, std::string acl, bool mark,KAccess *kaccess) {
	KChain *chain = findChain(index);
	if (chain==NULL) {
		return false;
	}
	if (mark) {
		return chain->addMark(acl,"",kaccess)!=NULL;
	} else {
		return chain->addAcl(acl,"",kaccess)!=NULL;
	}
}
bool KTable::downModel(int index, std::string acl, bool mark)
{
	KChain *chain = findChain(index);
	if (chain==NULL) {
		return false;
	}
	if (mark) {
		return chain->downMark(acl);
	} else {
		return chain->downAcl(acl);
	}
}
bool KTable::delAcl(int index, std::string acl, bool mark) {
	KChain *chain = findChain(index);
	if (chain==NULL) {
		return false;
	}
	if (mark) {
		return chain->delMark(acl);
	} else {
		return chain->delAcl(acl);
	}
}
KChain *KTable::findChain(const char *name)
{
	std::map<std::string,KChain *>::iterator it;
	it = chain_map.find(name);
	if (it==chain_map.end()) {
		return NULL;
	}
	return (*it).second;
}
KChain *KTable::findChain(int index)
{
	if (index==-1) {
		return end;
	}
	KChain *chain = head;
	while (index-->0 && chain) {
		chain = chain->next;
	}
	return chain;
}
void KTable::chainChangeName(std::string oname,KChain *chain)
{
	if (chain->name==oname) {
		return;
	}
	std::map<std::string,KChain *>::iterator it;
	if (oname.size()>0) {
		it = chain_map.find(oname);
		if (it!=chain_map.end() && chain==(*it).second) {
			chain_map.erase(it);
		}
	}
	if (chain->name.size()>0) {
		it = chain_map.find(chain->name);
		if (it==chain_map.end()) {
			chain_map.insert(std::pair<std::string,KChain *>(chain->name,chain));
		}
	}
}
