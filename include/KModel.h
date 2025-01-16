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
#ifndef KMODEL_H_INCLUDE
#define KMODEL_H_INCLUDE
#include <memory>
#include "KConfigTree.h"
#include "serializable.h"

#define MODEL_ACL 	1
#define MODEL_MARK	2
typedef uint32_t kgl_jump_type;
class KAccess;
class KFetchObject;
class KHttpObject;
class KHttpRequest;
using KSafeSource = std::unique_ptr<KFetchObject>;
/*
 * 控制模块基类
 */
class KModel {
public:
	KModel() {
		//revers = false;		
		//is_or = false;
		isGlobal = true;
		ref = 1;
	}
	virtual const char *getName() = 0;
	/* 工厂实例不会调用 parse_config */
	virtual void parse_config(const khttpd::KXmlNodeBody* xml) = 0;
	virtual void parse_child(const kconfig::KXmlChanged* changed) {
	}
	virtual void get_display(KWStream& s) = 0;
	virtual void get_html(KWStream& s) = 0;
	KModel* add_ref() {
		katom_inc((void*)&ref);
		return this;
	}
	uint32_t get_ref() {
		return katom_get((void*)&ref);
	}
	void release() {
		if (katom_dec((void*)&ref) == 0) {
			delete this;
		}
	}
	void dump(kgl::serializable* m, bool is_short) {
		//m->add("revers", revers);
		//m->add("is_or", is_or);
		m->add("module", getName());
		KStringBuf out;
		if (is_short) {
			get_display(out);
		} else {
			get_html(out);
		}
		m->add("html", out.str());
	}
	/* 命名模块的名字 */
	//KString named;
	//bool revers;
	//bool is_or;
	bool isGlobal;
protected:
	volatile uint32_t ref;
	virtual ~KModel() {
	}
};
using KSafeModel = KSharedObj<KModel>;

#endif /*KMODEL_H_*/
