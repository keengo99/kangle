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
#include "KXmlSupport.h"
#include "KHtmlSupport.h"
#include "KConfigTree.h"
#include "KCountable.h"

#define MODEL_ACL 	1
#define MODEL_MARK	2
typedef int kgl_jump_type;
class KAccess;
class WhmContext;
/*
 * 控制模块基类
 */
class KModel {
public:
	KModel() {
		revers = false;
		isGlobal = true;
		is_or = false;
		ref = 1;
	}
	virtual const char *getName() = 0;
	virtual int whmCall(WhmContext *ctx)
	{
		return 500;
	}
	virtual std::string getDisplay() = 0;
	virtual void parse_config(const khttpd::KXmlNodeBody* xml) = 0;
	virtual void parse_child(const kconfig::KXmlChanged* changed) {
	}
	virtual std::string getHtml(KModel* model) = 0;
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
	std::string name;
	bool revers;
	bool is_or;
	bool isGlobal;
protected:
	volatile uint32_t ref;
	virtual ~KModel() {
	}
};

#endif /*KMODEL_H_*/
