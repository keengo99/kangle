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
class WhmExtendCall {
public:
	int whmCall(const char* eventType, WhmContext* context);
	WhmExtend *extend;
	whm_call_ptr call_ptr = nullptr;
	KString callName;
	bool force = false;
};
class WhmCallMap {
public:
	WhmCallMap(WhmExtend *callable, const KString &callName);
	virtual ~WhmCallMap();
	int call(WhmContext *context);
	void addBeforeEvent(WhmExtend *event, KXmlAttribute&attribute);
	void addAfterEvent(WhmExtend *event, KXmlAttribute&attribute);
	WhmCallScope scope;
	KString name;
private:
	WhmExtendCall *makeEvent(KXmlAttribute&attribute);
	std::list<WhmExtendCall *> beforeEvents;
	std::list<WhmExtendCall *> afterEvents;
	WhmExtendCall *callable;
};

#endif /* WHMCALLMAP_H_ */
