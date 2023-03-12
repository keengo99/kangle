/*
 * WhmUrl.h
 *
 *  Created on: 2010-9-1
 *      Author: keengo
 */

#ifndef WHMURL_H_
#define WHMURL_H_

#include "WhmExtend.h"

class WhmUrl: public WhmExtend {
public:
	WhmUrl(const KString&file);
	virtual ~WhmUrl();
	bool init(KString &whmFile) override {
		return true;
	}
	bool startElement(KXmlContext* context) override {
		return true;
	}
	int call(const char *callName,const char *eventType, WhmContext *context) override;
	const char *getType() override
	{
		return "url";
	}
};

#endif /* WHMURL_H_ */
