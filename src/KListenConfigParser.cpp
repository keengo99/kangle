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

KSafeListen parse_listen(const KXmlAttribute& attribute) {
	KSafeListen m_host(new KListenHost);
	m_host->ip = attribute["ip"];
	m_host->port = attribute["port"];
#ifdef KSOCKET_SSL
	m_host->parse_certificate(attribute);
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
		return nullptr;
	}
	return m_host;
}
void on_listen_file_event(std::vector<KSafeListen>& listen, kconfig::KConfigEvent* ev) {
	auto locker = KVirtualHostManage::locker();
	auto dlisten = KVirtualHostManage::get_listen();
	uint32_t old_count = ev->diff->old_to - ev->diff->from;
	for (uint32_t index = ev->diff->from; index < ev->diff->new_to; ++index) {
		auto lh = parse_listen(ev->new_xml->get_body(index)->attributes);		
		if (lh) {
			dlisten->AddGlobal(lh.get());
		}
		listen.insert(listen.begin() + index + old_count, std::move(lh));		
	}
	//remove
	for (uint32_t index = ev->diff->from; index < ev->diff->old_to; ++index) {
		assert(ev->diff->from < listen.size());
		if (listen[ev->diff->from]) {
			dlisten->RemoveGlobal(listen[ev->diff->from].get());
		}
		listen.erase(listen.begin()+ ev->diff->from);
	}
	listen.shrink_to_fit();
}
void on_listen_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	
	assert(ev->get_xml()->is_tag(_KS("listen")));
	assert(!KBIT_TEST(ev->type, kconfig::EvSubDir));
	if (!ev->get_xml()->is_tag(_KS("listen"))) {
		return;
	}
	auto file_name = ev->file->get_name();
	auto result = conf.services.emplace(std::piecewise_construct, std::tuple<const char *>(file_name->data), std::tuple<>());
	auto it = result.first;
	on_listen_file_event((*it).second, ev);
	if ((*it).second.size()==0) {
		conf.services.erase(it);
	}
}
