/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include <string.h>
#include <stdlib.h>
#include <vector>
#include "KContentType.h"
#include "KXml.h"
#include "kmalloc.h"
#include "KVirtualHostManage.h"
#if 0
KContentType contentType;
using namespace std;
KContentType::KContentType() {
}

KContentType::~KContentType() {
	destroy();
}
const char *KContentType::get(KHttpObject *obj,const char *ext) {
	/*
	bool result = false;
	if(ext==NULL){
		return defaultMimeType.c_str();
	}
	map<char *,mime_type *,lessp_icase>::iterator it;
	it = mimetypes.find((char *)ext);
	if (it != mimetypes.end()) {
		mime_type *m_type = (*it).second;
		result = true;
		if(m_type->gzip){
			KBIT_SET(obj->index.flags,FLAG_NEED_GZIP);
		}
		if (m_type->max_age>0) {
			obj->index.max_age = m_type->max_age;
			KBIT_SET(obj->index.flags,ANSW_HAS_MAX_AGE);
		}
		return m_type->type;
	}
	return defaultMimeType.c_str();
	*/
	return "";
}
void KContentType::destroy() {
}
bool KContentType::load(std::string mimeFile) {
	FILE *fp = fopen(mimeFile.c_str(),"rb");
	if (fp==NULL) {
		return false;
	}
	fclose(fp);
	KXml xml;
	xml.setEvent(this);
	return xml.parseFile(mimeFile);
}
bool KContentType::startElement(KXmlContext *context, std::map<std::string,
		std::string> &attribute) {
	if (context->parent == NULL) {
		conf.gvm->globalVh.addMimeType("*",attribute["default"].c_str(),false,0);
	} else if (context->qName == "file") {
		bool compress = false;
		if (!attribute["gzip"].empty()) {
			compress = attribute["gzip"] == "1";
		}
		if (!attribute["compress"].empty()) {
			compress = attribute["compress"] == "1";
		}
		conf.gvm->globalVh.addMimeType(attribute["ext"].c_str(),attribute["type"].c_str(), compress, atoi(attribute["max_age"].c_str()));
	}
	return true;
}
bool KContentType::startCharacter(KXmlContext *context, char *character,
		int len) {
	return true;
}
#endif
