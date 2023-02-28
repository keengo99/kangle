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
	if (attribute["http3"] == "1") {
		KBIT_SET(m_host->alpn, KGL_ALPN_HTTP3);
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
void on_file_event(khttpd::KAutoArray<KListenHost>& listen, kconfig::KConfigEvent* ev) {
	auto locker = KVirtualHostManage::locker();
	auto dlisten = KVirtualHostManage::get_listen();
	uint32_t old_count = ev->diff.old_to - ev->diff.from;
	for (uint32_t index = ev->diff.from; index < ev->diff.new_to; ++index) {
		auto lh = parse_listen(ev->new_xml->get_body(index)->attributes);		
		if (lh == nullptr) {
			continue;
		}
		lh->ext = !ev->file->is_default();
		bool result = listen.insert(lh, index + old_count);
		assert(result);
		if (!result) {
			delete lh;
			continue;
		}
		dlisten->AddGlobal(lh);
	}
	//remove
	for (uint32_t index = ev->diff.from; index < ev->diff.old_to; ++index) {
		auto lh = listen.remove(index);
		if (lh==nullptr) {
			continue;
		}
		dlisten->RemoveGlobal(lh);
		delete lh;
	}
}
void on_listen_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	
	assert(ev->get_xml()->is_tag(_KS("listen")));
	assert(!KBIT_TEST(ev->type, kconfig::EvSubDir));
	if (!ev->get_xml()->is_tag(_KS("listen"))) {
		return;
	}
	auto file_name = ev->file->get_name();
	auto it = conf.services.find(file_name->data);
	if (it == conf.services.end()) {
		auto result = conf.services.emplace(file_name->data, nullptr);
		it = result.first;
	}
	on_file_event((*it).second, ev);
	if ((*it).second.size()==0) {
		conf.services.erase(it);
	}
}
