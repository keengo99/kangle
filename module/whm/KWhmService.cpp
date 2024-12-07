/*
 * KWhmService.cpp
 *
 *  Created on: 2010-8-30
 *      Author: keengo
 */

#include "KWhmService.h"
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
		KHttpPost httpPost;
		httpPost.init((int)contentLength);		
		if(!httpPost.readData(provider->getInputStream())){
			return false;
		}
		httpPost.parse(uv);
	}
	provider->sendUnknowHeader("Cache-Control","no-cache,no-store");
	context.buildVh();
	WhmPackage *package = packageManage.load(provider->getFileName());
	if (package == NULL) {
		provider->sendStatus(STATUS_NOT_FOUND, "Not Found");
		return true;
	}
	provider->sendStatus(STATUS_OK, "OK");
	const char *callName = uv->getx("whm_call");
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
	if(callName){
		if (fmt != kgl::format::json) {
			*out << "<" << callName << " whm_version=\"1.0\">";
		} else {
			*out << "{\"call\":\"" << callName << "\",";
		}
		int ret = package->process(callName,&context);
		context.flush(ret, fmt);
		if (fmt != kgl::format::json) {
			*out << "</" << callName << ">\n";
		} else {
			*out << "}";
		}
	}else{
		if (fmt != kgl::format::json) {
			*out << "<result whm_version=\"1.0\">whm_call cann't be empty</result>";
		} else {
			*out << "{\"status\":\"404\",\"result\":\"whm_call cann't be empty\"}";
		}
	}
	package->release();
	return true;
}
