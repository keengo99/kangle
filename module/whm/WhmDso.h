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
typedef int (WINAPI * WhmCallf)(const char *callName, const char *eventType,
		WHM_CONTEXT *context);
typedef BOOL (WINAPI * WhmTerminatef)(DWORD dwFlags);
class WhmDso: public WhmExtend {
public:
	WhmDso(std::string &file);
	virtual ~WhmDso();
	bool init(std::string &whmFile);
	int call(const char *callName, const char *eventType, WhmContext *context);
	const char *getType()
	{
		return "dso";
	}
private:
	HMODULE handle;
	GetWhmVersionf GetWhmVersion;
	WhmCallf WhmCall;
	WhmTerminatef WhmTerminate;
};

#endif /* WHMDSO_H_ */
