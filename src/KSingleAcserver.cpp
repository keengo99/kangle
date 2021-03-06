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

KSingleAcserver::KSingleAcserver(KSockPoolHelper *nodes)
{
	sockHelper = nodes;
}
KSingleAcserver::KSingleAcserver() {
	sockHelper = new KSockPoolHelper;
}
KSingleAcserver::~KSingleAcserver() {
	sockHelper->release();
}
void KSingleAcserver::set_proto(Proto_t proto)
{
	this->proto = proto;
	sockHelper->set_tcp(kangle::is_upstream_tcp(proto));
}
KUpstream* KSingleAcserver::GetUpstream(KHttpRequest* rq)
{
	return sockHelper->get_upstream(rq->get_upstream_flags(), rq->sink->data.raw_url->host);
}
bool KSingleAcserver::setHostPort(std::string host, const char *port) {
	return sockHelper->setHostPort(host , port);
}
void KSingleAcserver::buildXML(std::stringstream &s) {
	s << "\t<server name='" << name << "' proto='";
	s << KPoolableRedirect::buildProto(proto);
	s << "'";
	std::map<std::string, std::string> params;
	sockHelper->build(params);
	build_xml(params, s);
	s << "/>\n";
}
