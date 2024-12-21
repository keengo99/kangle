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
#ifndef KMULTINACL_H_
#define KMULTINACL_H_

#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "utils.h"
#include "krbtree.h"
typedef int (* strncmpfunc)(const char *,const char *,size_t);
typedef int (* strcmpfunc)(const char *,const char *);
struct multin_item
{
	char *str;
	int len;
};
class KMultinAcl: public KAcl {
public:
	KMultinAcl() {
		seticase(true);
		split = '|';
		icase_can_change = true;
		root.rb_node = NULL;
	}
	virtual ~KMultinAcl() {
		freeMap();
	}
	void get_html(KWStream &s) override {
		s << "<input name=v size=40 value='";
		KMultinAcl *acl = (KMultinAcl *) (this);
		if (acl) {
			acl->getValList(s);
		}
		s << "'>";
		if(icase_can_change){
			s << "<input type=checkbox name=icase value='1' ";
			if (acl == NULL || acl->icase) {
				s << "checked";
			}
			s << ">ignore case,";
		}	
		s << "split:<input name=split max=1 size=1 value='";
		if (acl == NULL) {
			s << "|";
		} else {
			s << acl->split;
		}
		s << "'>";
	}
	void getValList(KWStream &s) {
		krb_node *node;
		bool isFirst = true;
		for (node=rb_first(&root); node!=NULL; node=rb_next(node)) {
			if (!isFirst) {
				s << split;
			}
			isFirst = false;
			s << ((multin_item *)node->data)->str;
		}
	}
	void get_display(KWStream &s) override {
		if (icase) {
			s << "[I]";
		}
		getValList(s);
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		freeMap();
		if(icase_can_change){
			if (attribute["icase"] == "1") {
				seticase(true);
			} else {
				seticase(false);
			}
		}
		if (attribute["split"].size() > 0) {
			split = attribute["split"][0];
		} else {
			split = '|';
		}
		if (attribute["v"].size() > 0) {
			explode(attribute["v"].c_str());
		} else {
			auto text = xml->get_text();
			if (!text.empty()) {
				explode(text.c_str());
			}
		}
	}
protected:
	bool match(const char *item) {
		if (item == NULL) {
			return false;
		}
		struct krb_node **n = &(root.rb_node);
		multin_item *data;
		while (*n) {
			data = (multin_item *)((*n)->data);
			int result = ncmp(item,data->str,data->len);
			if (result < 0)
				n = &((*n)->rb_left);
			else if (result > 0)
				n = &((*n)->rb_right);
			else
				return true;
		}
		return false;
	}
	virtual char *transferItem(char *item)
	{
		return item;
	}
protected:
	void seticase(bool icase)
	{
		this->icase = icase;
		if(icase){
			ncmp = strncasecmp;
			cmp = strcasecmp;
		} else {
			ncmp = strncmp;
			cmp = strcmp;
		}
	}
	void insert(char *item)
	{
		struct krb_node **n = &(root.rb_node), *parent = NULL;
		multin_item *data;
		while (*n) {
			data = (multin_item *)((*n)->data);
			int result = cmp(item,data->str);
			parent = *n;
			if (result < 0)
				n = &((*n)->rb_left);
			else if (result > 0)
				n = &((*n)->rb_right);
			else{
				free(item);
				return;
			}
		}
		data = new multin_item;
		data->str = item;
		data->len = strlen(item);

		krb_node *node = new krb_node;
		node->data = data;
		rb_link_node(node, parent, n);
		rb_insert_color(node, &root);
	}
	void explode(const char *str)
	{
		char *buf = strdup(str);
		char *hot = buf;
		while(hot){
			char *p = strchr(hot,split);
			if (p) {
				*p = '\0';
			}
			if(*hot){
				char *item = transferItem(strdup(hot));
				insert(item);
			}
			if (p) {
				hot = p+1;
			} else {
				hot = NULL;
			}			
		}
		free(buf);
	}	
	void freeMap() {
		for(;;){
			krb_node *node = rb_first(&root);
			if(node==NULL){
				break;
			}
			assert(node->data);
			multin_item *data = (multin_item *)node->data;
			free(data->str);
			delete data;
			rb_erase(node,&root);
			delete node;
		}		
	}
	strncmpfunc ncmp;
	strcmpfunc cmp;
	char split;
protected:
	bool icase;
	bool icase_can_change;
	krb_root root;
};

#endif /*KFILEEXEACL_H_*/
