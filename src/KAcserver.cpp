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
#include <string.h>
#include "KAcserver.h"
#include "KSockFastcgiFetchObject.h"
#include "KHttpProxyFetchObject.h"
#include "KAjpFetchObject.h"
#include "KTcpFetchObject.h"
#include "KSockPoolHelper.h"
#include "KLang.h"
#include "kmalloc.h"
using namespace std;
KJump::~KJump() {
}
KPoolableRedirect::KPoolableRedirect() {
	proto = Proto_http;
}
KPoolableRedirect::~KPoolableRedirect() {

}
KSockPoolHelper *KPoolableRedirect::parse_nodes(const char *node_string)
{
	char *buf = strdup(node_string);
	char *hot = buf;
	KSockPoolHelper *nodes = NULL;
	while (*hot) {
		char *p = strchr(hot, ',');
		if (p) {
			*p = '\0';
		}
		char *port = NULL;
		if (*hot == '[') {
			hot++;
			port = strchr(hot, ']');
			if (port) {
				*port = '\0';
				port++;
				port = strchr(port, ':');
			}
		} else {
			port = strchr(hot, ':');
		}
		if (port) {
			*port = '\0';
			port++;
			KSockPoolHelper *sockHelper = new KSockPoolHelper;
			sockHelper->next = NULL;
			sockHelper->prev = NULL;
			sockHelper->setHostPort(hot, port);
			char *lifeTime = strchr(port, ':');
			if (lifeTime) {
				*lifeTime = '\0';
				lifeTime++;
				sockHelper->setLifeTime(atoi(lifeTime));
				char *weight = strchr(lifeTime, ':');
				if (weight) {
					*weight = '\0';
					weight++;
					sockHelper->weight = atoi(weight);
					char *ip = strchr(weight, ':');
					if (ip) {
						*ip = '\0';
						ip++;
						sockHelper->setIp(ip);
					}
				}
			}
			if (nodes == NULL) {
				nodes = sockHelper;
			} else {
				sockHelper->next = nodes;
				nodes = sockHelper;
			}			
		}
		if (p == NULL) {
			break;
		}
		hot = p + 1;
	}
	free(buf);
	return nodes;
}
void KPoolableRedirect::build_proto_html(KPoolableRedirect *mserver, std::stringstream &s)
{
#ifndef HTTP_PROXY
	s << klang["protocol"] << ":";
	s << "<input type='radio' name='proto' value='http' ";
	if (mserver == NULL || mserver->proto == Proto_http) {
		s << "checked";
	}
	s << ">http <input type='radio' name='proto' value='fastcgi' ";
	if (mserver && mserver->proto == Proto_fcgi) {
		s << "checked";
	}
	s << ">fastcgi ";
	s << "<input type='radio' value='ajp' name='proto' ";
	if (mserver && mserver->proto == Proto_ajp) {
		s << "checked";
	}
	s << ">ajp";
#endif
#if 0
	s << "<input type='radio' value='uwsgi' name='proto' ";
	if (mserver && mserver->proto == Proto_uwsgi) {
		s << "checked";
	}
	s << ">uwsgi";
	s << "<input type='radio' value='scgi' name='proto' ";
	if (mserver && mserver->proto == Proto_scgi) {
		s << "checked";
	}
	s << ">scgi";
	s << "<input type='radio' value='hmux' name='proto' ";
	if (mserver && mserver->proto == Proto_hmux) {
		s << "checked";
	}
	s << ">hmux";
#endif
	//{{ent
#ifdef WORK_MODEL_TCP
	//tcp
	s << "<input type='radio' name='proto' value='tcp' ";
	if (mserver && mserver->proto == Proto_tcp) {
		s << "checked";
	}
	s << ">tcp";
#endif//}}
#ifdef ENABLE_PROXY_PROTOCOL
	//proxy
	s << "<input type='radio' name='proto' value='proxy' ";
	if (mserver && mserver->proto == Proto_proxy) {
		s << "checked";
	}
	s << ">proxy";
#endif
	s << "<br>";

}
KFetchObject *KPoolableRedirect::makeFetchObject(KHttpRequest *rq, KFileName *file) {
	CLR(rq->filter_flags,RQ_FULL_PATH_INFO);
	switch (proto) {
	case Proto_fcgi:
		return new KFastcgiFetchObject();
	case Proto_http:
		return new KHttpProxyFetchObject();
	case Proto_ajp:
		return new KAjpFetchObject();
#if 0
	case Proto_uwsgi:
		return new KUwsgiFetchObject();
	case Proto_scgi:
		return new KScgiFetchObject();
	case Proto_hmux:
		return new KHmuxFetchObject();
#endif
		//{{ent
#ifdef WORK_MODEL_TCP
	case Proto_tcp:
		return new KTcpFetchObject(false);
#endif//}}
#ifdef ENABLE_PROXY_PROTOCOL
	case Proto_proxy:
		return new KTcpFetchObject(true);
#endif
	default:
		return NULL;
	}
	return NULL;
}

