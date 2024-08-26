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
#ifndef KMULTIACL_H_
#define KMULTIACL_H_

#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "utils.h"
#include "krbtree.h"
typedef int (*strcmpfunc)(const char*, const char*);
class KMultiAcl : public KAcl {
public:
	KMultiAcl() {
		seticase(true);
		split = '|';
		icase_can_change = true;
		root.rb_node = NULL;
	}
	virtual ~KMultiAcl() {
		freeMap();
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "<input name=v size=40 value='";
		KMultiAcl* acl = (KMultiAcl*)(model);
		if (acl) {
			acl->getValList(s);
		}
		s << "'>";
		if (icase_can_change) {
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
	void getValList(KWStream& s) {
		krb_node* node;
		bool isFirst = true;
		for (node = rb_first(&root); node != NULL; node = rb_next(node)) {
			if (!isFirst) {
				s << split;
			}
			isFirst = false;
			s << (char*)node->data;
		}
		return;
	}
	void get_display(KWStream& s) override {
		if (icase) {
			s << "[I]";
		}
		getValList(s);
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		freeMap();
		if (icase_can_change) {
			if (xml->attributes["icase"] == "1") {
				seticase(true);
			} else {
				seticase(false);
			}
		}
		if (xml->attributes["split"].size() > 0) {
			split = xml->attributes["split"][0];
		} else {
			split = '|';
		}
		auto text = xml->get_text();
		if (!text.empty()) {
			explode(text.c_str());
		}
		if (!xml->attributes["v"].empty()) {
			explode(xml->attributes["v"].c_str());
		}
	}
protected:
	bool match(const char* item) {
		if (item == NULL) {
			return false;
		}
		struct krb_node** n = &(root.rb_node);
		char* data;
		while (*n) {
			data = (char*)((*n)->data);
			int result = cmp(item, data);
			if (result < 0)
				n = &((*n)->rb_left);
			else if (result > 0)
				n = &((*n)->rb_right);
			else
				return true;
		}
		return false;
	}
	virtual char* transferItem(char* item) {
		return item;
	}
private:
	void insert(char* item) {
		struct krb_node** n = &(root.rb_node), * parent = NULL;
		char* data;
		while (*n) {
			data = (char*)((*n)->data);
			int result = cmp(item, data);
			parent = *n;
			if (result < 0)
				n = &((*n)->rb_left);
			else if (result > 0)
				n = &((*n)->rb_right);
			else {
				free(item);
				return;
			}
		}
		krb_node* node = new krb_node;
		node->data = item;
		rb_link_node(node, parent, n);
		rb_insert_color(node, &root);
	}
	void explode(const char* str) {
		char* buf = strdup(str);
		char* hot = buf;
		while (hot) {
			char* p = strchr(hot, split);
			if (p) {
				*p = '\0';
			}
			if (*hot) {
				char* item = transferItem(strdup(hot));
				insert(item);
			}
			if (p) {
				hot = p + 1;
			} else {
				hot = NULL;
			}
		}
		free(buf);
	}
	void freeMap() {
		for (;;) {
			krb_node* node = rb_first(&root);
			if (node == NULL) {
				break;
			}
			assert(node->data);
			free(node->data);
			rb_erase(node, &root);
			delete node;
		}
	}
	strcmpfunc cmp;
	char split;
protected:
	void seticase(bool icase) {
		this->icase = icase;
		if (icase) {
			cmp = strcasecmp;
		} else {
			cmp = strcmp;
		}
	}
	bool icase;
	bool icase_can_change;
	krb_root root;
};

#endif /*KFILEEXEACL_H_*/
