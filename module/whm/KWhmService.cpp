/*
 * KWhmService.cpp
 *
 *  Created on: 2010-8-30
 *      Author: keengo
 */

#include "KWhmService.h"
#include <ctype.h>
#include <string.h>
#include "WhmContext.h"
#include "WhmPackageManage.h"
#include "KHttpPost.h"

KWhmService::KWhmService() {
	// TODO Auto-generated constructor stub

}

KWhmService::~KWhmService() {
	// TODO Auto-generated destructor stub
}
bool KWhmService::service(KServiceProvider *provider) {
	WhmContext context(provider);
	KUrlValue *uv = context.getUrlValue();
	uv->parse(provider->getQueryString());
	INT64 contentLength = provider->getContentLength();
	if(contentLength > 0){
		if (contentLength > MAX_POST_SIZE) {
			return false;
		}
		KHttpPost httpPost;
		if(!httpPost.init((int)contentLength)){
			return false;
		}
		if(!httpPost.readData(provider->getInputStream())){
			return false;
		}
		httpPost.parse(uv);
	}
	provider->sendUnknowHeader("Cache-Control","no-cache,no-store");
	{
		/* set by the server for admin-port requests; clients cannot send it */
		char value[8];
		int len = sizeof(value);
		context.trusted = provider->getEnv("KANGLE_MANAGE", value, &len) && strcmp(value, "1") == 0;
	}
	context.buildVh();
	WhmPackage *package = packageManage.load(provider->getFileName());
	if (package == NULL) {
		provider->sendStatus(STATUS_NOT_FOUND, "Not Found");
		return true;
	}
	provider->sendStatus(STATUS_OK, "OK");
	const char *callName = uv->getx("whm_call");
	if (callName) {
		/* it is echoed as an XML tag name and a JSON string below */
		for (const char* p = callName; *p; p++) {
			if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-' && *p != '.') {
				callName = NULL;
				break;
			}
		}
		if (callName && *callName == '\0') {
			callName = NULL;
		}
	}
	const char* v = uv->getx("format");
	kgl::format fmt = kgl::format::xml_text;
	if (v) {
		if (strcasecmp(v, "json") == 0) {
			fmt = kgl::format::json;
		} else if (strcasecmp(v, "xml-attribute") == 0) {
			fmt = kgl::format::xml_attribute;
		}
	}
	KWStream *out = provider->getOutputStream();
	if (fmt!= kgl::format::json) {
		provider->sendUnknowHeader("Content-Type", "text/xml");
		*out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
	} else {
		provider->sendUnknowHeader("Content-Type", "application/json");
	}
	try {
		if (callName) {
			if (fmt != kgl::format::json) {
				*out << "<" << callName << " whm_version=\"1.0\">";
			} else {
				*out << "{\"call\":\"" << callName << "\",";
			}
			int ret = package->process(callName, &context);
			context.flush(ret, fmt);
			if (fmt != kgl::format::json) {
				*out << "</" << callName << ">\n";
			} else {
				*out << "}";
			}
		} else {
			if (fmt != kgl::format::json) {
				*out << "<result whm_version=\"1.0\">whm_call cann't be empty</result>";
			} else {
				*out << "{\"status\":\"404\",\"result\":\"whm_call cann't be empty\"}";
			}
		}
	} catch (...) {
	}
	package->release();
	return true;
}
