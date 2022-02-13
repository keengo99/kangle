/*
 * KSingleAcServer.cpp
 *
 *  Created on: 2010-6-4
 *      Author: keengo
 */
#include "do_config.h"
#include "KSingleAcserver.h"
#include "KAsyncFetchObject.h"

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
KUpstream* KSingleAcserver::GetUpstream(KHttpRequest* rq)
{
	return sockHelper->GetUpstream(rq);
}
bool KSingleAcserver::setHostPort(std::string host, const char *port) {
	return sockHelper->setHostPort(host , port);
}
void KSingleAcserver::buildXML(std::stringstream &s) {
	s << "\t<server name='" << name << "' proto='";
	s << KPoolableRedirect::buildProto(proto);
	s << "'";
	sockHelper->buildXML(s);
	s << "/>\n";
}
