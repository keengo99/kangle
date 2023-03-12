/*
 * WhmPackage.h
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */

#ifndef WHMPACKAGE_H_
#define WHMPACKAGE_H_
#include <map>
#include "KServiceProvider.h"
#include "KXmlEvent.h"
#include "KCountable.h"
#include "WhmCallMap.h"
/*
 * whm包,由配置文件生成的whm调用及event信息.
 */
class WhmPackage: public KXmlEvent, public KCountable {
public:
	WhmPackage();
	virtual ~WhmPackage();
	int process(const char *callName,WhmContext *context);
	int getInfo(WhmContext *context);
	void flush();
	int query(WhmContext *context);
	int terminate(WhmContext *context);
	void setFile(const char *file)
	{
		this->file = file;
	}
	bool startElement(KXmlContext *context) override;
	bool startCharacter(KXmlContext *context, char *character, int len) override;
	bool endElement(KXmlContext *context) override;
private:
//	WhmContext *whmContext;
	WhmCallMap *newCallMap(const KString &name, const KString &callName);
	WhmExtend *findExtend(const KString &name);
	WhmCallMap *findCallMap(const KString &name);
	/*
	 * 调用映射
	 */
	std::map<KString, WhmCallMap *> callmap;
	std::map<KString, WhmExtend *> extends;
	WhmCallMap *curCallable;
	WhmExtend *curExtend;
	KString file;
	KString version;
};

#endif /* WHMPACKAGE_H_ */
