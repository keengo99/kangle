/*
 * WhmDso.h
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */

#ifndef WHMDSO_H_
#define WHMDSO_H_

#include "WhmExtend.h"
typedef BOOL (WINAPI * GetWhmVersionf)(WHM_VERSION_INFO *pVer);
typedef int (WINAPI * WhmCallf)(const char *callName, const char *eventType, WHM_CONTEXT *context);
typedef BOOL (WINAPI * WhmTerminatef)(DWORD dwFlags);
class WhmDso: public WhmExtend {
public:
	WhmDso(KString &file);
	virtual ~WhmDso();
	bool init(KString &whmFile) override;
	int call(const char *callName, const char *eventType, WhmContext *context) override;
	const char *getType() override
	{
		return "dso";
	}
	bool startElement(KXmlContext* context) override {
		return true;
	}
private:
	HMODULE handle;
	GetWhmVersionf GetWhmVersion;
	WhmCallf WhmCall;
	WhmTerminatef WhmTerminate;
};

#endif /* WHMDSO_H_ */
