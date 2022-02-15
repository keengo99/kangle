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
#include <iostream>
#include "KListenConfigParser.h"
#include "KXml.h"
#include "KAccess.h"
#include "KAcserverManager.h"
#include "KWriteBackManager.h"
#include "kmalloc.h"
#include "kthread.h"
#include "KRequestQueue.h"
#include "lib.h"
#include "do_config.h"
using namespace std;
KListenConfigParser listenConfigParser;
KWorkerConfigParser worker_config_parser;
bool KWorkerConfigParser::startCharacter(KXmlContext *context, char *character, int len)
{
	if (context->path == "config" && context->qName == "worker_thread") {
		conf.select_count = atoi(character);
		return true;
	}
	return false;
}
bool KWorkerConfigParser::parse(std::string file) {
	KXml xmlParser;
	xmlParser.setEvent(this);
	bool result = false;
	try {
		result = xmlParser.parseFile(file);
	}catch(KXmlException e) {
		return false;
	}
	return result;
}
bool KListenConfigParser::startElement(std::string &context, std::string &qName,
		std::map<std::string, std::string> &attribute) {
	if (context!="config") {
		return true;
	}
	KConfig *c = cconf;
	if (c==NULL) {
		c = &conf;
	}
	if (qName == "listen") {
		KListenHost *m_host = new KListenHost;
		//m_host->name = attribute["name"];
		m_host->ip = attribute["ip"];
		m_host->port = attribute["port"];
#ifdef KSOCKET_SSL
		m_host->cert_file = attribute["certificate"];
		m_host->key_file = attribute["certificate_key"];
#ifdef ENABLE_HTTP2
		m_host->http2 = attribute["http2"]=="1";
#endif
		m_host->early_data = attribute["early_data"] == "1";
		m_host->cipher = attribute["cipher"];
		m_host->protocols = attribute["protocols"];
#endif
		if (!parseWorkModel(attribute["type"].c_str(),m_host->model)) {
			delete m_host;
			return false;
		}
		c->service.push_back(m_host);
		return true;
	}
	return true;
}
bool KListenConfigParser::startCharacter(std::string &context, std::string &qName,
		char *character, int len) {
	if (context == "config") {
		
	}
	return true;
}
bool KListenConfigParser::parse(std::string file) {
	KXml xmlParser;
	xmlParser.setEvent(this);
	bool result = false;
	result = xmlParser.parseFile(file);
	return result;
}
