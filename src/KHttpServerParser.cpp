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
#include "katom.h"
#include <vector>
#include <string.h>
#include <stdlib.h>

#include <assert.h>
#include "KHttpServerParser.h"
#include "KHttpProxyFetchObject.h"
#include "KLogElement.h"
#include "KVirtualHost.h"
#include "KVirtualHostManage.h"
#include "KLogManage.h"
#include "KAcserverManager.h"
#include "KTempleteVirtualHost.h"
#include "KXml.h"
#include "utils.h"
#include "KVirtualHostDatabase.h"
#include "extern.h"
#include "kmalloc.h"
#ifndef HTTP_PROXY
using namespace std;
KHttpServerParser::KHttpServerParser() {

}
KHttpServerParser::~KHttpServerParser() {

}
bool KHttpServerParser::buildBaseVirtualHost(KXmlAttribute &attribute, KBaseVirtualHost *bv) {
	//兼容老的index 处理
	map<string, string>::iterator it;
	std::vector<std::string> indexFiles;
	std::vector<KBaseString> indexFiles2;
	it = attribute.find("index");
	if (it != attribute.end()) {
		explode((*it).second.c_str(), ',', indexFiles);
		for (size_t i = 0; i < indexFiles.size(); i++) {
			//indexFiles2.push_back(indexFiles[i]);
			bv->addIndexFile(indexFiles[i]);
		}
	}
	map<int, KBaseString> errorPages;
	for (it = attribute.begin(); it != attribute.end(); it++) {
		if (strncasecmp((*it).first.c_str(), "error_", 6) == 0) {
			//errorPages[atoi((*it).first.c_str() + 6)] = (*it).second;
			bv->addErrorPage(atoi((*it).first.c_str() + 6),(*it).second);
		}
	}
	return true;
}
bool KHttpServerParser::buildVirtualHost(KAttributeHelper *ah, KVirtualHost *virtualHost, KBaseVirtualHost *gvh,KVirtualHost *ov) {
	return true;
}
KVirtualHost *KHttpServerParser::buildVirtualHost(KXmlAttribute&attribute,KBaseVirtualHost *gvh, KVirtualHost *ov) {
	KAttributeHelper helper(attribute);
	return buildVirtualHost(&helper,gvh, ov);
}
KVirtualHost *KHttpServerParser::buildVirtualHost(KAttributeHelper *ah,KBaseVirtualHost *gvh, KVirtualHost *ov) {
	KVirtualHost *virtualHost = new KVirtualHost("");
	if (buildVirtualHost(ah, virtualHost, gvh,ov)) {
		return virtualHost;
	}
	delete virtualHost;
	return NULL;
}
bool KHttpServerParser::ParseSsl(KVirtualHostManage *vhm, std::map<std::string,std::string> &attribute)
{
	std::string domain = attribute["domain"];
	if (domain.empty()) {
		klog(KLOG_ERR, "set ssl failed domain is empty\n", domain.c_str());
		return false ;
	}
	if (!vhm->BindSsl(domain.c_str(), attribute["certificate"].c_str(), attribute["certificate_key"].c_str())) {
		klog(KLOG_ERR, "set ssl for domain [%s] failed\n", domain.c_str());
		return false;
	}
	return true;
}
#endif
