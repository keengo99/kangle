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
#ifndef kacserver_h_skdfjs999sfkh1lk2j3
#define kacserver_h_skdfjs999sfkh1lk2j3
#include <string>
#include <list>
#include "ksocket.h"
#include "KMutex.h"
#include "KJump.h"
#include "KFetchObject.h"
#include "KRedirect.h"
#include "KUpstream.h"
#include "KFileName.h"
class KRedirectSource;
class KSockPoolHelper;
class KPoolableRedirect: public KRedirect {

public:
	KPoolableRedirect();
	virtual ~KPoolableRedirect();
	KRedirectSource*makeFetchObject(KHttpRequest *rq, KFileName *file) override;
	virtual bool isChanged(KPoolableRedirect *rd)
	{
		return this->proto != rd->proto;
	}
	virtual void shutdown() = 0;
	void remove() {
		shutdown();
		release();
	}
	bool startElement(KXmlContext* context) override {
		return true;
	}
	static void build_xml(std::map<std::string, std::string> &param, std::stringstream& s);
	static void build_proto_html(KPoolableRedirect *m_a, std::stringstream &s);
	static KSockPoolHelper *parse_nodes(const char *node_string);
	static const char *buildProto(Proto_t proto) {
		switch (proto) {
		case Proto_http:
			return "http";
		case Proto_fcgi:
			return "fastcgi";
		case Proto_ajp:
			return "ajp";
#if 0
		case Proto_uwsgi:
			return "uwsgi";
		case Proto_scgi:
			return "scgi";
		case Proto_hmux:
			return "hmux";
#endif
		case Proto_spdy:
			return "spdy";
		case Proto_tcp:
			return "tcp";
#ifdef ENABLE_PROXY_PROTOCOL
		case Proto_proxy:
			return "proxy";
#endif
		}
		return "unknow";
	}
	static Proto_t parseProto(const char *proto) {
		if (strcasecmp(proto, "fcgi") == 0 || strcasecmp(proto, "fastcgi") == 0) {
			return Proto_fcgi;
		}
		if (strcasecmp(proto, "ajp") == 0) {
			return Proto_ajp;
		}
#if 0
		if (strcasecmp(proto,"uwsgi")==0) {
			return Proto_uwsgi;
		}
		if (strcasecmp(proto,"scgi")==0) {
			return Proto_scgi;
		}
		if (strcasecmp(proto,"hmux")==0) {
			return Proto_hmux;
		}
#endif
		if (strcasecmp(proto, "tcp") == 0) {
			return Proto_tcp;
		}
#ifdef ENABLE_PROXY_PROTOCOL
		if (strcasecmp(proto, "proxy") == 0) {
			return Proto_proxy;
		}
#endif
		return Proto_http;
	}
	Proto_t get_proto()
	{
		return proto;
	}
	virtual void set_proto(Proto_t proto) = 0;
protected:
	Proto_t proto;
};
#endif
