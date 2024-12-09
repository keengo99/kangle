/*
 * KMultiAcserver.cpp
 *
 *  Created on: 2010-6-4
 *      Author: keengo
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
#include <sstream>
#include "KMultiAcserver.h"
#include "global.h"
#include "lang.h"
#include "kmalloc.h"
#include "KAsyncFetchObject.h"
#include "KAcserverManager.h"
#include "KConfig.h"
#include "HttpFiber.h"
using namespace std;
#ifdef ENABLE_MULTI_SERVER
bool KMultiAcserver::on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	auto xml = ev->get_xml();
	if (!xml->is_tag(_KS("node"))) {
		klog(KLOG_NOTICE, "Warning unknow xml tag [%s]\n", xml->key.tag->data);
		return false;
	}
	switch (ev->type) {
	case kconfig::EvNew | kconfig::EvSubDir:
	case kconfig::EvUpdate | kconfig::EvSubDir:
	{
		KLocker locker(&lock);
		removeAllNode();
		for (uint32_t index = 0;; index++) {
			auto body = xml->get_body(index);
			if (!body) {
				break;
			}
			KSockPoolHelper* node = new KSockPoolHelper;
			if (node->parse(body->attributes)) {
				addNode(node);
			} else {
				node->release();
			}
		}
		buildVNode();
		return true;
	}
	case kconfig::EvRemove: {
		KLocker locker(&lock);
		removeAllNode();
		buildVNode();
		break;
	}
	default:
		break;
	}
	return true;
}
void KMultiAcserver::init() {
	nodes = NULL;
	nodesCount = 0;
	ip_hash = false;
	url_hash = false;
	cookie_stick = false;
	errorTryTime = 20;
	max_error_count = 5;
#ifdef ENABLE_MSERVER_ICP
	icp = NULL;
#endif
}
KMultiAcserver::KMultiAcserver(const KString &name) : KPoolableRedirect(name) {
	init();
}
KMultiAcserver::KMultiAcserver(KSockPoolHelper* nodes) : KPoolableRedirect(""){
	init();
	while (nodes) {
		KSockPoolHelper* next = nodes->next;
		addNode(nodes);
		nodes = next;
	}
	buildVNode();
}
KMultiAcserver::~KMultiAcserver() {
	removeAllNode();
#ifdef ENABLE_MSERVER_ICP
	if (icp) {
		icp->release();
	}
#endif
}
void KMultiAcserver::removeAllNode() {
	KSockPoolHelper* helper = nodes;
	while (helper) {
		KSockPoolHelper* n = helper->next;
		helper->stopMonitor();
		helper->release();
		nodesCount--;
		if (n == nodes) {
			break;
		}
		helper = n;
	}
	nodes = NULL;
	bnodes.clear();
	vnodes.clear();
	assert(nodesCount == 0);
	nodesCount = 0;
}
void KMultiAcserver::enableAllServer() {
	KSockPoolHelper* helper = nodes;
	while (helper) {
		KSockPoolHelper* n = helper->next;
		helper->enable();
		if (n == nodes) {
			break;
		}
		helper = n;
	}
}
int KMultiAcserver::getCookieStick(const char* attr, size_t len, const char* cookie_stick_name) {
	char* buf = kgl_strndup(attr, len);
	if (buf == NULL) {
		return -1;
	}
	int cookie_stick_value = -1;
	char* hot = buf;
	while (*hot) {
		char* p = strchr(hot, ';');
		if (p) {
			*p = '\0';
		}
		while (*hot && isspace((unsigned char)*hot)) {
			hot++;
		}
		char* eq = strchr(hot, '=');
		if (eq) {
			*eq = '\0';
			if (strcmp(hot, cookie_stick_name) == 0) {
				cookie_stick_value = atoi(eq + 1) - 1;
				break;
			}
		}
		if (p == NULL) {
			break;
		}
		hot = p + 1;
	}
	free(buf);
	return cookie_stick_value;
}
uint16_t KMultiAcserver::getNodeIndex(KHttpRequest* rq, int* set_cookie_stick) {
	if (cookie_stick) {
		const char* cookie_stick_name = conf.cookie_stick_name;
		if (*cookie_stick_name == '\0') {
			cookie_stick_name = DEFAULT_COOKIE_STICK_NAME;
		}
		KHttpHeader* av = rq->sink->data.get_header();
		while (av) {
			if (kgl_is_attr(av, _KS("Cookie"))) {
				int cookie_stick_value = getCookieStick(av->buf + av->val_offset, av->val_len, cookie_stick_name);
				if (cookie_stick_value >= 0) {
					if (cookie_stick_value < (int)vnodes.size()) {
						return (unsigned short)cookie_stick_value;
					}
					break;
				}
			}
			av = av->next;
		}
	}
	unsigned index;
	if (url_hash) {
		index = string_hash(rq->sink->data.url->host, 8, 1);
		index = string_hash(rq->sink->data.url->path, 16, index);
	} else if (ip_hash) {
		index = string_hash(rq->getClientIp(), MAXIPLEN - 1, 1);
	} else {
		index = rand();
	}
	index = index % vnodes.size();
	if (cookie_stick) {
		*set_cookie_stick = index + 1;
	}
	return (unsigned short)index;
}
static KUpstream* connect_result(KHttpRequest* rq, KSockPoolHelper* sa, int cookie_stick) {
	if (cookie_stick > 0 && !KBIT_TEST(rq->sink->data.flags, RQ_UPSTREAM_ERROR)) {
		KStringBuf b;
		if (*conf.cookie_stick_name) {
			b << conf.cookie_stick_name;
		} else {
			b.WSTR(DEFAULT_COOKIE_STICK_NAME);
		}
		b.WSTR("=");
		b << cookie_stick;
		b.WSTR("; path=/");
		rq->response_header(_KS("Set-Cookie"), b.buf(), b.size());
	}
	rq->ctx.upstream_sign = sa->sign;
	KUpstream* us = sa->get_upstream(rq->get_upstream_flags(), rq->sink->data.raw_url.host);
	sa->release();
	return us;
}

KUpstream* KMultiAcserver::GetUpstream(KHttpRequest* rq) {
	//KAsyncFetchObject *afo = static_cast<KAsyncFetchObject *>(fo);
	KSockPoolHelper* sockHelper = NULL;
	int set_cookie_stick = 0;
	lock.Lock();
	if (!vnodes.empty()) {
		uint16_t index = getNodeIndex(rq, &set_cookie_stick);
		sockHelper = vnodes[index];
		if (sockHelper->is_enabled()) {
			//the node is active
			sockHelper->addRef();
			lock.Unlock();
			return connect_result(rq, sockHelper, set_cookie_stick);
		}
		//look for next active node
		KSockPoolHelper* na = nextActiveNode(sockHelper, index);
		if (na) {
			na->addRef();
			lock.Unlock();
			if (cookie_stick) {
				set_cookie_stick = index + 1;
			}
			return connect_result(rq, na, set_cookie_stick);
		}
	}
	KSockPoolHelper* fast_node = NULL;
	for (size_t i = 0; i < bnodes.size(); i++) {
		//look for fastest backup node
		KSockPoolHelper* bnode = bnodes[i];
		if (bnode->is_enabled()) {
			if (fast_node == NULL || bnode->avg_monitor_tick < fast_node->avg_monitor_tick) {
				fast_node = bnode;
				continue;
			}
		}
	}
	if (fast_node != NULL) {
		fast_node->addRef();
		lock.Unlock();
		return connect_result(rq, fast_node, 0);
	}
#ifdef ENABLE_MSERVER_ICP
	if (icp) {
		//use icp
		KPoolableRedirect* icp_rd = icp;
		icp_rd->add_ref();
		lock.Unlock();
		KUpstream* us = icp_rd->GetUpstream(rq);
		icp_rd->release();
		return us;
	}
#endif
	//reset all node
	enableAllServer();
	if (sockHelper) {
		sockHelper->addRef();
		lock.Unlock();
		return connect_result(rq, sockHelper, set_cookie_stick);
	}
	lock.Unlock();
	return NULL;
}

KSockPoolHelper* KMultiAcserver::nextActiveNode(KSockPoolHelper* node, unsigned short& index) {
	KSockPoolHelper* helper = node;
	bool use_next = (index & 1) > 0;
	while (helper) {
		KSockPoolHelper* n = (use_next) ? helper->next : helper->prev;
		if (use_next) {
			if (index >= vnodes.size() - 1) {
				index = 0;
			} else {
				index++;
			}
		} else {
			if (index == 0) {
				index = (unsigned short)vnodes.size() - 1;
			} else {
				index--;
			}
		}
		if (!helper->disable_flag) {
			return helper;
		}
		if (n == node) {
			return NULL;
		}
		helper = n;
	}
	return NULL;
}

unsigned KMultiAcserver::getPoolSize() {
	return 0;
}
bool KMultiAcserver::addNode(const KXmlAttribute& attr) {
	KSockPoolHelper* sockHelper = new KSockPoolHelper;
	if (!sockHelper->parse(attr)) {
		delete sockHelper;
		return false;
	}
	lock.Lock();
	addNode(sockHelper);
	buildVNode();
	lock.Unlock();
	return true;
}
bool KMultiAcserver::addNode(KXmlAttribute& attr, char* self_ip) {
	char* to_ip = strchr(self_ip, '-');
	if (to_ip) {
		*to_ip = '\0';
		to_ip++;
		sockaddr_i min_addr;
		sockaddr_i max_addr;
		if (!ksocket_getaddr(self_ip, 0, AF_INET, AI_NUMERICHOST, &min_addr)) {
			//if (!KSocket::getaddr(self_ip, 0, &min_addr, AF_INET, AI_NUMERICHOST)) {			
			return false;
		}
		if (!ksocket_getaddr(to_ip, 0, AF_INET, AI_NUMERICHOST, &max_addr)) {
			return false;
		}
		uint32_t min_ip = ntohl(min_addr.v4.sin_addr.s_addr);
		uint32_t max_ip = ntohl(max_addr.v4.sin_addr.s_addr);
		for (uint32_t ip = min_ip; ip <= max_ip; ip++) {
			min_addr.v4.sin_addr.s_addr = htonl(ip);
			char ips[MAXIPLEN];
			ksocket_sockaddr_ip(&min_addr, ips, sizeof(ips));
			//KSocket::make_ip(&min_addr, ips, MAXIPLEN);
			attr.emplace("self_ip", ips);
			addNode(attr);
		}
		return true;
	}
	return false;
}
bool KMultiAcserver::editNode(KXmlAttribute& attr) {
	auto  action = attr["action"];
	if (action == "edit") {
		int id = atoi(attr["id"].c_str());
		lock.Lock();
		KSockPoolHelper* node = getIndexNode(id);
		if (node) {
			node->weight = atoi(attr["weight"].c_str());
			node->parse(attr);
			buildVNode();
		}
		lock.Unlock();
	} else {
		const char* self_ip = attr["self_ip"].c_str();
		if (self_ip && *self_ip) {
			char* buf = strdup(self_ip);
			if (addNode(attr, buf)) {
				free(buf);
				return true;
			}
			free(buf);
		}
		return addNode(attr);
	}
	return true;
}
void KMultiAcserver::addNode(KSockPoolHelper* sockHelper) {
	sockHelper->set_tcp(kangle::is_upstream_tcp(proto));
	sockHelper->setErrorTryTime(max_error_count, errorTryTime);
	if (nodes == NULL) {
		sockHelper->next = sockHelper;
		sockHelper->prev = sockHelper;
		nodes = sockHelper;
	} else {
		sockHelper->next = nodes;
		sockHelper->prev = nodes->prev;
		nodes->prev->next = sockHelper;
		nodes->prev = sockHelper;
	}
	nodesCount++;
}
void  KMultiAcserver::nodeForm(KWStream& s, const KString &name, KMultiAcserver* as,unsigned nodeIndex) {

	KSockPoolHelper* helper = NULL;
	if (as) {
		as->lock.Lock();
		helper = as->getIndexNode((int)nodeIndex);
	}
	s << "<form action='/macserver_node?name=" << name << "&action=";
	if (helper) {
		s << "edit";
	} else {
		s << "add";
	}
	s << "&id=" << nodeIndex << "' method='post'>\n";
	s << klang["lang_host"] << ": <input name='host' value='";
	if (helper) {
		s << helper->host;
	}
	s << "'>";
	s << LANG_PORT << ": <input name='port' size='5' value='";
	if (helper) {
		s << helper->get_port();
	}
	s << "'><br>";
	s << klang["lang_life_time"] << ": <input name='life_time' size=6 value='" << (helper ? helper->getLifeTime() : 0) << "'><br>";

#ifdef HTTP_PROXY
	s << LANG_USER << ": <input name='auth_user' value='"
		<< (helper ? helper->auth_user.c_str() : "") << "'><br>";
	s << LANG_PASS << ": <input name='auth_passwd' value='"
		<< (helper ? helper->auth_passwd.c_str() : "") << "'><br>";
#endif

	s << klang["weight"] << ": <input name='weight' value='" << (helper ? helper->weight : 1) << "'><br>";
	const char* self_ip = NULL;
	if (helper) {
		self_ip = helper->getIp();
	}
	s << "self_ip: <input name='self_ip' value='" << (self_ip ? self_ip : "") << "'><br>";
	kgl_refs_string* param = NULL;
	if (helper) {
		param = helper->GetParam();
	}
	s << "param:<input name='param' value='";
	if (param) {
		s.write_all(param->data, param->len);
	}
	kstring_release(param);
	s << "'><br>";
	s << "<input type=checkbox name='sign' value='1' " << (helper && helper->sign ? "checked" : "") << ">sign<br>";
	s << "<input type='submit' value='" << LANG_SUBMIT << "'>";
	s << "</form>";
	if (as) {
		as->lock.Unlock();
	}
}
KSockPoolHelper* KMultiAcserver::getIndexNode(int index) {
	if (index >= nodesCount) {
		return NULL;
	}
	KSockPoolHelper* helper = nodes;
	while (helper) {
		if (index-- <= 0) {
			return helper;
		}
		KSockPoolHelper* n = helper->next;
		helper = n;
	}
	return NULL;
}
bool KMultiAcserver::parse_config(const khttpd::KXmlNode* xml) {
	if (!KPoolableRedirect::parse_config(xml)) {
		return false;
	}
	auto& attributes = xml->attributes();
#ifdef ENABLE_MSERVER_ICP
	setIcp(attributes["icp"].c_str());
#endif
	lock.Lock();
	url_hash = attributes.get_int(_KS("url_hash")) == 1;
	if (!url_hash) {
		ip_hash = attributes["ip_hash"] == "1";
	} else {
		ip_hash = false;
	}
	cookie_stick = attributes["cookie_stick"] == "1";
	lock.Unlock();
	setErrorTryTime(attributes.get_int("max_error_count"), attributes.get_int("error_try_time"));
	return true;
}
void KMultiAcserver::shutdown() {
	lock.Lock();
	KSockPoolHelper* node = nodes;
	while (node) {
		node->shutdown();
		node = node->next;
		if (node == nodes) {
			break;
		}
	}
	lock.Unlock();
}
void KMultiAcserver::getNodeInfo(KWStream& s) {
	lock.Lock();
	KSockPoolHelper* node = nodes;
	for (int i = 0; i < nodesCount; i++) {
		if (i > 0) {
			s << "\n";
		}
		s << node->host;
		if (!node->is_unix) {
			s << ":" << node->port;
		}
		s << "\t" << node->total_connect << "/" << node->total_hit << "\t" << (node->is_enabled() ? "OK" : "FAILED");
		s << "(" << node->total_error << " " << node->avg_monitor_tick << ")";
		node = node->next;
	}
	lock.Unlock();
}
void KMultiAcserver::getHtml(KWStream & s) {
	lock.Lock();
	s << "<tr><td rowspan='" << nodesCount << "'>";
	s << "[<a href=\"javascript:if(confirm('really delete')){ window.location='/del_macserver?name="
		<< name << "';}\">" << LANG_DELETE << "</a>]";
	s << "[<a href='/extends?item=1&name=" << name << "'>";
	s << LANG_EDIT << "</a>]";
	s << "[<a href='/macserver_node_form?name=" << name << "&action=add'>"
		<< klang["add_node"] << "</a>]</td>";
	s << "<td rowspan='" << nodesCount << "'>" << name << "</td>";
	s << "<td rowspan='" << nodesCount << "'>";
	s << KPoolableRedirect::buildProto(proto);
	s << "</td>";
	s << "<td rowspan='" << nodesCount << "'>";
	if (url_hash) {
		s << "url";
	} else if (ip_hash) {
		s << "ip";
	} else {
		s << "rand";
	}
	s << "</td>";
	s << "<td rowspan='" << nodesCount << "'>" << (cookie_stick ? LANG_ON : "&nbsp;") << "</td>";
	s << "<td rowspan='" << nodesCount << "'>" << errorTryTime << "</td>";
	s << "<td rowspan='" << nodesCount << "'>" << max_error_count << "</td>";
	s << "<td rowspan='" << nodesCount << "'>" << get_ref() << "</td>";
#ifdef ENABLE_MSERVER_ICP
	s << "<td rowspan='" << nodesCount << "'>" << (icp ? icp->name : "&nbsp;") << "</td>";
#endif
	KSockPoolHelper* node = nodes;
	for (int i = 0; i < nodesCount; i++) {
		assert(node);
		if (i > 0) {
			s << "<tr>";
		}
		s << "<td>[<a href=\"javascript:if(confirm('delete this node?')){ window.location='/macserver_node?name="
			<< name << "&id=" << i << "&action=delete';}\">" << LANG_DELETE
			<< "</a>]";
		s << "[<a href='/macserver_node_form?name=" << name << "&id=" << i
			<< "&action=edit'>" << LANG_EDIT << "</a>]"
			<< node->host << "</td>";
		s << "<td>" << node->get_port() << "</td>";
		s << "<td>" << node->getLifeTime() << "</td>";
		s << "<td>" << node->getSize() << "</td>";
		s << "<td>" << node->weight << "</td>";
		const char* ip = node->getIp();
		s << "<td>" << (ip ? ip : "") << "</td>";
		s << "<td>" << node->total_connect << "/" << node->total_hit << "</td>";
		s << "<td>" << node->total_error << "/" << node->avg_monitor_tick << "</td>";
		s << "<td>" << (node->is_enabled() ? "<font color=green>OK</font>" : "<font color=red>FAILED</font>") << "</td>";
		s << "</tr>\n";
		node = node->next;
	}
	if (nodesCount == 0) {
		s << "<td colspan=9>&nbsp; </td></tr>";
	}
	lock.Unlock();
}
void KMultiAcserver::baseHtml(KMultiAcserver* mserver, KWStream& s) {
	KPoolableRedirect::build_proto_html(mserver, s);
	s << "<input type='checkbox' name='ip_hash' value='1' ";
	if (mserver && mserver->ip_hash) {
		s << "checked";
	}
	s << ">" << klang["ip_hash"];
	s << "<input type='checkbox' name='url_hash' value='1' ";
	if (mserver && mserver->url_hash) {
		s << "checked";
	}
	s << ">url_hash";
	s << "<input type='checkbox' name='cookie_stick' value='1' ";
	if (mserver && mserver->cookie_stick) {
		s << "checked";
	}
	s << ">" << klang["cookie_stick"] << "<br>";
	s << klang["error_try_time"] << "<input name='error_try_time' value='" << (mserver ? mserver->errorTryTime : 30) << "' size='4'><br>";
	s << klang["error_count"] << "<input name='max_error_count' value='" << (mserver ? mserver->max_error_count : 5) << "' size='4'><br>";
#ifdef ENABLE_MSERVER_ICP
	s << "icp" << "<input name='icp' value='";
	if (mserver && mserver->icp) {
		s << mserver->icp->name;
	}
	s << "' size='8'><br>";
#endif

}
void KMultiAcserver::form(KMultiAcserver* mserver, KWStream& s) {
	s << "<form action='/macserveradd?action=" << (mserver ? "edit" : "add") << "' method=post>\n";
	s << LANG_NAME<< ":<input name=name value='";
	if (mserver) {
		s << mserver->name;
	}
	s << "' ";
	if (mserver) {
		s << "readonly";
	}
	s << "><br>";
	baseHtml(mserver, s);
	s << "<input type=submit value=" << LANG_ADD << "></form>\n";
}
void KMultiAcserver::parseNode(const char* nodeString) {
	KSockPoolHelper* nodes = KPoolableRedirect::parse_nodes(nodeString);
	lock.Lock();
	removeAllNode();
	while (nodes) {
		KSockPoolHelper* next = nodes->next;
		addNode(nodes);
		nodes = next;
	}
	buildVNode();
	lock.Unlock();

}
void KMultiAcserver::setErrorTryTime(int max_error_count, int errorTryTime) {
	if (errorTryTime > 600) {
		errorTryTime = 600;
	}
	if (errorTryTime < 5) {
		errorTryTime = 5;
	}
	lock.Lock();
	this->max_error_count = max_error_count;
	this->errorTryTime = errorTryTime;
	KSockPoolHelper* helper = nodes;
	while (helper) {
		KSockPoolHelper* n = helper->next;
		n->setErrorTryTime(max_error_count, errorTryTime);
		helper->error_try_time = errorTryTime;
		helper->max_error_count = max_error_count;
		if (n == nodes) {
			break;
		}
		helper = n;
	}
	lock.Unlock();
}
#ifdef ENABLE_MSERVER_ICP
void KMultiAcserver::setIcp(const char* icp_name) {
	KPoolableRedirect* new_icp = NULL;
	if (icp_name && *icp_name) {
		new_icp = conf.gam->refsAcserver(icp_name);
	}
	lock.Lock();
	if (icp) {
		icp->release();
		icp = NULL;
	}
	icp = new_icp;
	lock.Unlock();
}
#endif
void KMultiAcserver::set_proto(Proto_t proto) {
	this->proto = proto;
	lock.Lock();
	KSockPoolHelper* node = nodes;
	while (node) {
		node->set_tcp(kangle::is_upstream_tcp(proto));
		node = node->next;
		if (node == nodes) {
			break;
		}
	}
	lock.Unlock();
}
void KMultiAcserver::parse(KXmlAttribute& attribute) {
#ifdef ENABLE_MSERVER_ICP
	setIcp(attribute["icp"].c_str());
#endif
	lock.Lock();
	url_hash = attribute["url_hash"] == "1";
	if (!url_hash) {
		ip_hash = attribute["ip_hash"] == "1";
	} else {
		ip_hash = false;
	}
	cookie_stick = attribute["cookie_stick"] == "1";
	lock.Unlock();
	set_proto(KPoolableRedirect::parseProto(attribute["proto"].c_str()));
	setErrorTryTime(atoi(attribute["max_error_count"].c_str()), atoi(attribute["error_try_time"].c_str()));
}
#if 0
bool KMultiAcserver::delNode(int nodeIndex) {
	bool result = false;
	lock.Lock();
	KSockPoolHelper* helper = nodes;
	while (helper) {
		if (nodeIndex-- <= 0) {
			break;
		}
		KSockPoolHelper* n = helper->next;
		helper = n;
	}
	if (helper) {
		nodesCount--;
		if (helper == nodes) {
			nodes = nodes->next;
		}
		if (helper == helper->next) {
			assert(nodesCount == 0);
			nodes = NULL;
		} else {
			helper->prev->next = helper->next;
			helper->next->prev = helper->prev;
		}
		helper->stopMonitor();
		helper->release();
		buildVNode();
	}
	lock.Unlock();
	return result;
}
#endif
void KMultiAcserver::buildVNode() {
	vnodes.clear();
	bnodes.clear();
	KSockPoolHelper* helper = nodes;
	while (helper) {
		KSockPoolHelper* n = helper->next;
		if (helper->weight == 0) {
			bnodes.push_back(helper);
		} else {
			for (int i = 0; i < helper->weight; i++) {
				vnodes.push_back(helper);
			}
		}
		if (n == nodes) {
			break;
		}
		helper = n;
	}
}
void KMultiAcserver::buildAttribute(KWStream& s) {
	s << "proto='";
	s << KPoolableRedirect::buildProto(proto);
	s << "' ip_hash='" << (ip_hash ? 1 : 0) << "' ";
	s << "url_hash='" << (url_hash ? 1 : 0) << "' ";
	s << "cookie_stick='" << (cookie_stick ? 1 : 0) << "' ";
	s << "error_try_time='" << errorTryTime << "' ";
	s << "max_error_count='" << max_error_count << "' ";
#ifdef ENABLE_MSERVER_ICP
	if (icp) {
		s << "icp='" << icp->name << "' ";
	}
#endif
}
bool KMultiAcserver::isChanged(KPoolableRedirect* rd) {
	if (KPoolableRedirect::isChanged(rd)) {
		return true;
	}
	KMultiAcserver* ma = static_cast<KMultiAcserver*>(rd);
	if (this->nodesCount != ma->nodesCount) {
		return true;
	}
	if (ma->ip_hash != this->ip_hash) {
		return true;
	}
	if (ma->url_hash != this->url_hash) {
		return true;
	}
	if (ma->cookie_stick != this->cookie_stick) {
		return true;
	}
	if (ma->errorTryTime != this->errorTryTime) {
		return true;
	}
	if (ma->max_error_count != this->max_error_count) {
		return true;
	}
	KSockPoolHelper* helper = nodes;
	KSockPoolHelper* maHelper = ma->nodes;
	while (helper) {
		assert(maHelper);
		if (helper->isChanged(maHelper)) {
			return true;
		}
		helper = helper->next;
		maHelper = maHelper->next;
		if (helper == nodes) {
			break;
		}
	}
#ifdef ENABLE_MSERVER_ICP
	return icp != ma->icp;
#endif
	return false;
}
#endif
