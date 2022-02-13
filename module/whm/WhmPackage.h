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
	bool startElement(KXmlContext *context,
			std::map<std::string, std::string> &attribute);
	bool startCharacter(KXmlContext *context, char *character, int len);
	bool endElement(KXmlContext *context);
private:
//	WhmContext *whmContext;
	WhmCallMap *newCallMap(std::string &name,std::string &callName);
	WhmExtend *findExtend(std::string &name);
	WhmCallMap *findCallMap(std::string &name);
	/*
	 * 调用映射
	 */
	std::map<std::string, WhmCallMap *> callmap;
	std::map<std::string, WhmExtend *> extends;
	WhmCallMap *curCallable;
	WhmExtend *curExtend;
	std::string file;
	std::string version;
};

#endif /* WHMPACKAGE_H_ */
