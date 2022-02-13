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
#ifndef KMODEL_H_
#define KMODEL_H_
#include "KXmlSupport.h"
#include "KHtmlSupport.h"
#include "KCountable.h"

#define MODEL_ACL 	1
#define MODEL_MARK	2
class KAccess;
class WhmContext;
/*
 * 控制模块基类
 */
class KModel: public KXmlSupport, public KHtmlSupport, public KCountableEx {
public:
	KModel() {
		revers = false;
		isGlobal = true;
		is_or = false;
	}
	virtual ~KModel() {
	}
	/*
	 * 加到chain的顺序，返回true加到后面,如果返回false就要加到前面
	 */
	virtual bool addEnd()
	{
		return true;
	}
	/*
	* 是否有运行时信息?
	*/
	virtual bool supportRuntime()
	{
		return false;
	}
	virtual const char *getName()=0;
	virtual int whmCall(WhmContext *ctx)
	{
		return 500;
	}
	virtual std::string getHtml(KModel *model)=0;
	virtual bool startElement(KXmlContext *context,std::map<std::string, std::string> &attribute) {
		editHtml(attribute, false);
		return true;
	}
	bool revers;
	bool is_or;
	bool isGlobal;
	//use for runtime model
	std::string name;
};

#endif /*KMODEL_H_*/
