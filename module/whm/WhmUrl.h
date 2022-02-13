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
	WhmUrl(std::string &file);
	virtual ~WhmUrl();
	bool init(std::string &whmFile) {
		return true;
	}
	int call(const char *callName,const char *eventType, WhmContext *context);
	const char *getType()
	{
		return "url";
	}
};

#endif /* WHMURL_H_ */
