/*
 * KSockPoolHelper.h
 *
 *  Created on: 2010-6-4
 *      Author: keengo
 */

#ifndef KSOCKPOOLHELPER_H_
#define KSOCKPOOLHELPER_H_
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
#include <map>
#include <string>
#include <list>
#include "ksocket.h"
#include "global.h"
#include "KCountable.h"
#include "KPoolableSocketContainer.h"
#include "KTcpUpstream.h"
#include "KHttpRequest.h"
#define ERROR_RECONNECT_TIME	600
/**
* 服务器扩展节点
*/
class KSockPoolHelper : public KPoolableSocketContainer {
public:
	KSockPoolHelper();
	virtual ~KSockPoolHelper();
	void isBad(KUpstream *st,BadStage stage)
	{
		katom_inc64((void *)&this->total_error);
		switch(stage){
		case BadStage_Connect:
		case BadStage_TrySend:
			error_count++;
			if (error_count>=max_error_count) {
				disable();
			}
			break;
		default:
			break;
		}		
	}
	bool isChanged(KSockPoolHelper *sh)
	{
		if (host!=sh->host) {
			return true;
		}
		if (port!=sh->port) {
			return true;
		}
#ifdef ENABLE_UPSTREAM_SSL
		if (ssl!=sh->ssl) {
			return true;
		}
		if (no_sni != sh->no_sni) {
			return true;
		}
#endif
		if (weight!=sh->weight) {
			return true;
		}
		if (is_unix!=sh->is_unix) {
			return true;
		}
		if (ip!=sh->ip || (ip && strcmp(ip,sh->ip)!=0)) {
			return true;
		}
		if (lifeTime != sh->lifeTime) {
			return true;
		}
		if (sign != sh->sign) {
			return true;
		}
		if (param != sh->param || (param && strcmp(param->str.data, sh->param->str.data) != 0)) {
			return true;
		}
		return false;
	}
	void isGood(KUpstream *st)
	{
		enable();
	}
	void start_monitor_call_back();
	//void syncCheckConnect();
	void setErrorTryTime(int max_error_count, int error_try_time)
	{
		lock.Lock();
		this->max_error_count = max_error_count;
		this->error_try_time = error_try_time;
		if (max_error_count > 0) {
			internalStopMonitor();
		} else {
			startMonitor();
		}
		lock.Unlock();
	}
	KUpstream* GetUpstream(KHttpRequest* rq);
	void checkActive();
	bool setHostPort(std::string host,int port,const char *ssl);
	bool setHostPort(std::string host, const char *port);
	void disable();
	void enable();
	bool isEnable();
	void setIp(const char *ip)
	{
		if (this->ip) {
			free(this->ip);
			this->ip = NULL;
		}
		if (ip && *ip) {
			this->ip = strdup(ip);
		}
	}
	const char *getIp()
	{
		return this->ip;
	}
	std::string host;
	uint64_t hit;
	uint16_t weight;
	uint16_t port;
	union {
		struct {
			uint32_t monitor : 1;
			uint32_t is_unix : 1;
			uint32_t sign : 1;
#ifdef ENABLE_UPSTREAM_SSL
			uint32_t no_sni : 1;
#endif
		};
		uint32_t flags;
	};
#ifdef ENABLE_UPSTREAM_SSL
	//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
	bool http2;
#endif//}}
	bool IsSslVerify()
	{
		return ssl == "S";
	}
	std::string ssl;
#endif
	//{{ent
#ifdef HTTP_PROXY
	std::string auth_user;
	std::string auth_passwd;
	void setProxyAuth(std::string user,std::string passwd);
	void buildHttpAuth(KWStream &s);
#endif
	void addHeader(KHttpRequest *rq,KHttpEnv *s);
	//}}
	int error_try_time;
	/*
	 * 连续错误连接次数，如果超过MAX_ERROR_COUNT次，就会认为是问题的。
	 * 下次试连接时间会从当前时间加ERROR_RECONNECT_TIME秒。
	 */
	uint16_t error_count;
	uint16_t max_error_count;
	/*
	 * 下次试连接时间，如果是0表示活跃的。
	 */
	time_t tryTime;
	bool try_numerichost_connect(KHttpRequest *rq, KTcpUpstream *socket,bool &need_name_resolved);
	bool connect_addr(KHttpRequest *rq, KTcpUpstream *socket,sockaddr_i &addr);
	void buildXML(std::stringstream &s);
	bool parse(std::map<std::string,std::string> &attr);
	void monitorNextTick();
	void stopMonitor()
	{
		lock.Lock();
		internalStopMonitor();
		lock.Unlock();
	}
	volatile uint64_t total_error;
	volatile uint64_t total_connect;
	INT64 monitor_start_time;
	int avg_monitor_tick;
	KSockPoolHelper *next;
	KSockPoolHelper *prev;
private:

	KUpstream *GetConnection(KHttpRequest *rq, bool &half, bool &need_name_resolved);
	KUpstream *newConnection(KHttpRequest *rq, bool &need_name_resolved);
	void startMonitor();
	void internalStopMonitor()
	{
		monitor = 0;
	}
	char *ip;
	KMutex lock;
#ifdef ENABLE_UPSTREAM_SSL
	SSL_CTX *ssl_ctx;
#endif
};

#endif /* KSOCKPOOLHELPER_H_ */
