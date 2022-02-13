/*
 * WhmCallMap.h
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */

#ifndef WHMCALLMAP_H_
#define WHMCALLMAP_H_
#include <list>
#include "whm.h"
#include "WhmExtend.h"
enum WhmCallScope
{
	callScopePublic,
	callScopePrivate
};
struct WhmExtendCall{
	WhmExtend *extend;
	std::string callName;
	bool force;
};
class WhmCallMap {
public:
	WhmCallMap(WhmExtend *callable,std::string &callName);
	virtual ~WhmCallMap();
	int call(WhmContext *context);
	void addBeforeEvent(WhmExtend *event,std::map<std::string,std::string> &attribute);
	void addAfterEvent(WhmExtend *event,std::map<std::string,std::string> &attribute);
	WhmCallScope scope;
	std::string name;
private:
	WhmExtendCall *makeEvent(std::map<std::string,std::string> &attribute);
	std::list<WhmExtendCall *> beforeEvents;
	std::list<WhmExtendCall *> afterEvents;
	WhmExtendCall *callable;
};

#endif /* WHMCALLMAP_H_ */
