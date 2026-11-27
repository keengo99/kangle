/*
 * KSingleAcServer.cpp
 *
 *  Created on: 2010-6-4
 *      Author: keengo
 */
#include "do_config.h"
#include "KSingleAcserver.h"
#include "KAsyncFetchObject.h"
#include "HttpFiber.h"

KSingleAcserver::KSingleAcserver(KSockPoolHelper *nodes) : KPoolableRedirect("")
{
	sockHelper = nodes;
}
KSingleAcserver::KSingleAcserver(const KString&name) : KPoolableRedirect(name) {
	sockHelper = new KSockPoolHelper;
}
KSingleAcserver::~KSingleAcserver() {
	sockHelper->release();
}
bool KSingleAcserver::parse_config(const khttpd::KXmlNode* node) {
	if (!KPoolableRedirect::parse_config(node)) {
		return false;
	}
	return sockHelper->parse(node->attributes(), default_life_time());
}
void KSingleAcserver::set_proto(Proto_t proto)
{
	this->proto = proto;
	sockHelper->set_tcp(kangle::is_upstream_tcp(proto));
}
KUpstream* KSingleAcserver::GetUpstream(KHttpRequest* rq)
{
	rq->ctx.upstream_sign = sockHelper->sign;
	const char* sni_host = rq->sink->data.raw_url.host;
	if (!KBIT_TEST(rq->ctx.filter_flags, RF_PROXY_RAW_URL)
		&& KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_REWRITED)) {
		sni_host = rq->sink->data.url->host;
	}
	return sockHelper->get_upstream(rq->get_upstream_flags(), sni_host);
}
bool KSingleAcserver::setHostPort(std::string host, const char *port) {
	return sockHelper->setHostPort(host , port);
}
void KSingleAcserver::dump(kgl::serializable* s) {
	s->add("name", name);
	sockHelper->dump(s);
}
