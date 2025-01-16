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
		isGlobal = true;
		ref = 1;
	}
	virtual const char *get_module() const = 0;
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
		m->add("refs", get_ref());
		m->add("module", get_module());
		KStringBuf out;
		if (is_short) {
			get_display(out);
		} else {
			get_html(out);
		}
		m->add("html", out.str());
	}
	bool isGlobal;
protected:
	volatile uint32_t ref;
	virtual ~KModel() {
	}
};
using KSafeModel = KSharedObj<KModel>;

#endif /*KMODEL_H_*/
