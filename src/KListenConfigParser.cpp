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
#include "do_config.h"
using namespace std;
KListenConfigParser listenConfigParser;

KListenHost* parse_listen(const KXmlAttribute& attribute) {
	KListenHost* m_host = new KListenHost;
	//m_host->name = attribute["name"];
	m_host->ip = attribute["ip"];
	m_host->port = attribute["port"];
#ifdef KSOCKET_SSL
	m_host->cert_file = attribute["certificate"];
	m_host->key_file = attribute["certificate_key"];
	if (!attribute["alpn"].empty()) {
		m_host->alpn = (u_char)atoi(attribute["alpn"].c_str());
	} else {
		m_host->alpn = 0;
	}
#ifdef ENABLE_HTTP2
	if (!attribute["http2"].empty()) {
		m_host->alpn = attribute["http2"] == "1";
	}
#endif
	m_host->early_data = attribute["early_data"] == "1";
	m_host->cipher = attribute["cipher"];
	m_host->protocols = attribute["protocols"];
#endif
	if (!parseWorkModel(attribute["type"].c_str(), m_host->model)) {
		delete m_host;
		return nullptr;
	}
	return m_host;
}
void on_listen_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	assert(ev->xml->is_tag(_KS("listen")));
	assert(!KBIT_TEST(ev->type, kconfig::EvSubDir));
	if (!ev->xml->is_tag(_KS("listen"))) {
		return;
	}
	auto xml_node = tree->node;
}
bool KListenConfigParser::startElement(KXmlContext *context) {
	if (context->getParentName()!="config") {
		return true;
	}
	KConfig *c = cconf;
	if (c==NULL) {
		c = &conf;
	}
	if (context->qName == "listen") {
		KListenHost *m_host = new KListenHost;
		//m_host->name = attribute["name"];
		m_host->ip = context->attribute["ip"];
		m_host->port = context->attribute["port"];
#ifdef KSOCKET_SSL
		m_host->cert_file = context->attribute["certificate"];
		m_host->key_file = context->attribute["certificate_key"];
		if (!context->attribute["alpn"].empty()) {
			m_host->alpn = (u_char)atoi(context->attribute["alpn"].c_str());
		} else {
			m_host->alpn = 0;
		}
#ifdef ENABLE_HTTP2
		if (!context->attribute["http2"].empty()) {
			m_host->alpn = context->attribute["http2"] == "1";
		}
#endif
		m_host->early_data = context->attribute["early_data"] == "1";
		m_host->cipher = context->attribute["cipher"];
		m_host->protocols = context->attribute["protocols"];
#endif
		if (!parseWorkModel(context->attribute["type"].c_str(),m_host->model)) {
			delete m_host;
			return false;
		}
		c->service.push_back(m_host);
		return true;
	}
	return true;
}
bool KListenConfigParser::startCharacter(KXmlContext *context,
		char *character, int len) {
	if (context->getParentName() == "config") {
		
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
