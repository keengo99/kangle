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
