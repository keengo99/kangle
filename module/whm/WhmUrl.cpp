/*
 * WhmUrl.cpp
 *
 *  Created on: 2010-9-1
 *      Author: keengo
 */

#include "WhmUrl.h"

WhmUrl::WhmUrl(std::string &file) {
	this->file = file;
}

WhmUrl::~WhmUrl() {
}
int WhmUrl::call(const char *callName, const char *eventType,
		WhmContext *context) {
	KServiceProvider *provider = context->getProvider();
	KStringBuf s;
	s << file.c_str() << "?whm_event=" << eventType << "&"
			<< provider->getQueryString();
	if (provider->execUrl(s.getString())) {
		return WHM_OK;
	}
	return WHM_CALL_FAILED;
}
