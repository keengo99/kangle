/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include <string>
#include "WhmContext.h"

#include "whm.h"
#include "global.h"
#include "KReportIp.h"
#include "KVirtualHostManage.h"
#include "KVirtualHostDatabase.h"
#include "KHttpManage.h"
#include "extern.h"
#include "kserver.h"
#include "cache.h"
#include "KHttpLib.h"
#include "KConfigBuilder.h"
#include "KAcserverManager.h"
#include "KTempleteVirtualHost.h"
#include "KHttpServerParser.h"
#include "kselector_manager.h"
#include "KCdnContainer.h"
#include "KDsoExtendManage.h"
#include "kaddr.h"
#include "KLogDrill.h"
#include "extern.h"
#include "ssl_utils.h"

enum{
	CALL_UNKNOW,
	CALL_INFO,
	CALL_ADD_VH,
	CALL_DEL_VH,
	CALL_EDIT_VH,
	CALL_INFO_VH,
	CALL_LIST_VH,
	CALL_LIST_LISTEN,
	CALL_LIST_INDEX,
	CALL_RELOAD,
	CALL_RELOAD_VH,
	CALL_RELOAD_VH_ACCESS,
	CALL_CHANGE_ADMIN_PASSWORD,
	CALL_KILL_PROCESS,
	CALL_WRITE_FILE,
	CALL_REBOOT,
	CALL_CHECK_VH_DB,
	CALL_UPDATE_VH,
	CALL_INFO_DOMAIN,
	CALL_LIST_TABLE,	
	CALL_CLEAN_CACHE,
	CALL_CACHE_INFO,
	CALL_CACHE_PREFETCH,
	CALL_CLEAN_ALL_CACHE,

	CALL_GET_LOAD,
	CALL_DUMP_FLOW,
	CALL_DUMP_LOAD,
	CALL_GET_CONNECTION,
	CALL_CHECK_SSL,
	CALL_VH_STAT,
	CALL_BLACK_LIST,
	CALL_REPORT_IP,
	CALL_SERVER_INFO,
	CALL_QUERY_DOMAIN,
	CALL_LOG_DRILL,
	CALL_LIST_DSO
};
using namespace std;
BOOL WINAPI GetWhmVersion(WHM_VERSION_INFO *pVer) {
	return TRUE;
}
static int parseCallName(const char *callName)
{
	if(callName==NULL){
		return CALL_UNKNOW;
	}
	switch(*callName){
		case 'a':				
			if(strcmp(callName,"add_vh")==0){
				return CALL_ADD_VH;
			}				
			break;
		case 'b':
			if (strcmp(callName,"black_list")==0) {
				return CALL_BLACK_LIST;
			}
			break;
		case 'c':
			if(strcmp(callName,"change_admin_password")==0){
				return CALL_CHANGE_ADMIN_PASSWORD;
			}
			if(strcmp(callName,"check_vh_db")==0){
				return CALL_CHECK_VH_DB;
			}
			if(strcmp(callName,"clean_cache")==0){
				return CALL_CLEAN_CACHE;
			}
			if (strcmp(callName,"check_ssl")==0) {
				return CALL_CHECK_SSL;
			}
			if (strcmp(callName,"cache_info")==0) {
				return CALL_CACHE_INFO;
			}
			if (strcmp(callName,"cache_prefetch")==0) {
				return CALL_CACHE_PREFETCH;
			}
			if (strcmp(callName, "clean_all_cache") == 0) {
				return CALL_CLEAN_ALL_CACHE;
			}
			break;
		case 'd':			
			if(strcmp(callName,"del_vh")==0){
				return CALL_DEL_VH;
			}			
#ifdef ENABLE_VH_FLOW
			if (strcmp(callName,"dump_flow")==0) {
				return CALL_DUMP_FLOW;
			}
#endif
			if (strcmp(callName,"dump_load")==0) {
				return CALL_DUMP_LOAD;
			}
			break;
		case 'e':
			if(strcmp(callName,"edit_vh")==0){
				return CALL_EDIT_VH;
			}
			break;
		case 'g':
#ifdef ENABLE_VH_FLOW
			if(strcmp(callName,"get_load")==0){
				return CALL_GET_LOAD;
			}
			if (strcmp(callName,"get_connection")==0) {
				return CALL_GET_CONNECTION;
			}

#endif
			break;
		case 'i':
			if(strcmp(callName,"info")==0){
				return CALL_INFO;
			}
			if(strcmp(callName,"info_vh")==0){
				return CALL_INFO_VH;
			}
			if(strcmp(callName,"info_domain")==0){
				return CALL_INFO_DOMAIN;
			}
			break;	
		case 'k':
			if(strcmp(callName,"kill_process")==0){
				return CALL_KILL_PROCESS;
			}
			break;
		case 'l':
			if(strcmp(callName,"list_vh")==0){
				return CALL_LIST_VH;
			}
			if(strcmp(callName,"list_table")==0){
				return CALL_LIST_TABLE;
			}
			if (strcmp(callName,"list_index")==0) {
				return CALL_LIST_INDEX;
			}
			if (strcmp(callName, "list_listen") == 0) {
				return CALL_LIST_LISTEN;
			}
			if (strcmp(callName, "list_dso") == 0) {
				return CALL_LIST_DSO;
			}
#ifdef ENABLE_LOG_DRILL
			if (strcmp(callName, "log_drill") == 0) {
				return CALL_LOG_DRILL;
			}
#endif
			break;
		case 'q':
			if (strcmp(callName, "query_domain") == 0) {
				return CALL_QUERY_DOMAIN;
			}
		case 'r':
			if(strcmp(callName,"reload_vh")==0){
				return CALL_RELOAD_VH;
			}
			if (strcmp(callName, "reload_vh_access") == 0) {
				return CALL_RELOAD_VH_ACCESS;
			}
			if (strcmp(callName,"reboot")==0) {
				return CALL_REBOOT;
			}
			if (strcmp(callName,"reload")==0) {
				return CALL_RELOAD;
			}
			if (strcmp(callName,"report_ip")==0) {
				return CALL_REPORT_IP;
			}
			break;
		case 's':
			if (strcmp(callName,"stat_vh")==0) {
				return CALL_VH_STAT;
			}
			if (strcmp(callName, "server_info") == 0) {
				return CALL_SERVER_INFO;
			}
			break;
		case 'u':
			if (strcmp(callName,"update_vh")==0) {
				return CALL_UPDATE_VH;
			}
			break;	
		case 'w':
			if(strcmp(callName,"write_file")==0){
				return CALL_WRITE_FILE;
			}
			break;
	}
	return CALL_UNKNOW;
}
static int getVhDomain(WhmContext *ctx)
{
	KUrlValue *uv = ctx->getUrlValue();
	string name = uv->get("name");
	KVirtualHost *vh = conf.gvm->refsVirtualHostByName(name);
	if(vh==NULL){
		ctx->setStatus("vh cann't find");
		return WHM_PARAM_ERROR;
	}
	list<KSubVirtualHost *>::iterator it;
	for (it = vh->hosts.begin(); it != vh->hosts.end(); it++) {
		if(!(*it)->allSuccess){
			continue;
		}
		ctx->add("domain",(*it)->host);
	}
	vh->getParsedFileExt(ctx);
	vh->destroy();
	return WHM_OK;
}
#ifdef ENABLE_BLACK_LIST
static int getVhStat(WhmContext *ctx)
{
	KUrlValue *uv = ctx->getUrlValue();
	KVirtualHost *vh = ctx->getVh();
	if(vh==NULL){
		ctx->setStatus("vh cann't find");
		return WHM_PARAM_ERROR;
	}
	ctx->add("name",vh->name);
#ifdef ENABLE_VH_RS_LIMIT
	ctx->add("connect",vh->GetConnectionCount());
	ctx->add("speed",vh->get_speed(uv->get("reset")=="1"));
#ifdef ENABLE_VH_QUEUE
	if (vh->queue) {
		ctx->add("queue",vh->queue->getQueueSize());
		ctx->add("worker",vh->queue->getWorkerCount());
	}
#endif
	if (vh->blackList) {
		INT64 total_error_upstream, total_request, total_upstream;
		vh->blackList->getStat(total_request, total_error_upstream,total_upstream, uv->get("reset")=="1");
		ctx->add("total_error_upstream", total_error_upstream);
		ctx->add("total_upstream", total_upstream);
		ctx->add("total_request",total_request);
	}
#endif
	return WHM_OK;
}
#endif
static int getVhDetail(WhmContext *ctx)
{
	KUrlValue *uv = ctx->getUrlValue();
	string name = uv->get("name");
	KVirtualHost *vh = conf.gvm->refsVirtualHostByName(name);
	if(vh==NULL){
		ctx->setStatus("vh cann't find");
		return WHM_PARAM_ERROR;
	}
	//ctx->setStatus(WHM_OK);
	stringstream s;
	ctx->add("name",name);
#ifdef ENABLE_BASED_PORT_VH
	{
		list<string>::iterator it2;
		for (it2=vh->binds.begin();it2!=vh->binds.end();it2++) {
			s << (*it2) << "\n";
		}
	}
	ctx->add("bind",s.str().c_str());
	s.str("");
#endif
	{
		list<KSubVirtualHost *>::iterator it;
		for (it = vh->hosts.begin(); it != vh->hosts.end(); it++) {
			s << (*it)->host;
			if (strcmp((*it)->dir, "/") != 0) {
				s << "|" << (*it)->dir;
			}
			s << "\n";
		}
	}
	ctx->add("host",s.str().c_str());
	s.str("");
	ctx->add("doc_root",vh->GetDocumentRoot().c_str());
	ctx->add("inherit",vh->inherit?"1":"0");
#ifdef ENABLE_VH_RUN_AS
	ctx->add("user",vh->user);
#ifdef _WIN32
	ctx->add("password","***");
#else
	ctx->add("group",vh->group);
#endif
#endif

#ifdef ENABLE_VH_LOG_FILE
	ctx->add("log_file",vh->logFile);
	string rotateTime;
	if (vh->logger) {
		vh->logger->getRotateTime(rotateTime);
		ctx->add("log_rotate_time",rotateTime);
		ctx->add("log_rotate_size",vh->logger->rotate_size);
	}
#endif
	ctx->add("browse",vh->browse?"1":"0");
#ifdef ENABLE_USER_ACCESS
	ctx->add("access_file",vh->user_access);
#endif
#ifdef ENABLE_VH_RS_LIMIT
	ctx->add("connect",vh->max_connect);
	ctx->add("speed_limit",vh->speed_limit);
	//ctx->add("flow_limit",vh->flow_limit);
	//ctx->add("flow",vh->flow);
#endif
	vh->destroy();
	return WHM_OK;
}
int WINAPI WhmCoreCall(const char *callName, const char *event, WHM_CONTEXT *context) {
	WhmContext *ctx = (WhmContext *) context->ctx;
	//KWStream *out = ctx->getOutputStream();
	KUrlValue *uv = ctx->getUrlValue();
	std::string errMsg;
	int cmd = parseCallName(callName);
	switch(cmd) {
	case CALL_INFO:
		{		
			INT64 total_mem_size = 0, total_disk_size = 0;
			int mem_count = 0, disk_count = 0;
			cache.getSize(total_mem_size, total_disk_size,mem_count,disk_count);
			ctx->add("server", PROGRAM_NAME);
			ctx->add("version", VERSION);
			ctx->add("type",getServerType());
			ctx->add("os",getOsType());
			int total_run_time = (int)(kgl_current_sec - kgl_program_start_sec);
			ctx->add("total_run",total_run_time);
			ctx->add("connect",total_connect);
#ifdef ENABLE_STAT_STUB
			ctx->add("request",katom_get64((void *)&kgl_total_requests));
			ctx->add("accept",katom_get64((void *)&kgl_total_accepts));
#endif
			ctx->add("cache_count", cache.getCount());
			ctx->add("cache_mem", total_mem_size);
#ifdef ENABLE_DISK_CACHE
			INT64 total_size, free_size;
			ctx->add("cache_disk", total_disk_size);
			KStringBuf s;
			if (dci) {
				get_disk_base_dir(s);
				if (get_disk_size(total_size, free_size)) {
					ctx->add("disk_total", total_size);
					ctx->add("disk_free", free_size);
				}
			}
			ctx->add("disk_cache_dir", s.c_str());
#endif
			int vh_count = conf.gvm->getCount();
			ctx->add("vh",vh_count);
			ctx->add("kangle_home",conf.path.c_str());
#ifdef UPDATE_CODE
			ctx->add("update_code", UPDATE_CODE);
#endif
			ctx->add("open_file_limit", open_file_limit);
			ctx->add("addr_cache", kgl_get_addr_cache_count());
			ctx->add("disk_cache_shutdown", (int)cache.is_disk_shutdown());
			return WHM_OK;
		}
#if 0
	case CALL_ADD_VH:
		{
			if(!vhd.addVirtualHost(uv->attribute,ctx,errMsg)){
				ctx->setStatus(errMsg.c_str());
				return WHM_CALL_FAILED;
			}
			return WHM_OK;
		}
	case CALL_UPDATE_VH:
		{
			if(!vhd.updateVirtualHost(ctx,uv->attribute,errMsg)){
				ctx->setStatus(errMsg.c_str());
				return WHM_CALL_FAILED;
			}
			return WHM_OK;
		}
#endif
	case CALL_QUERY_DOMAIN:
	{
		const char *domain = uv->getx("domain");
		if (domain == NULL) {
			ctx->setStatus("missing domain");
			return WHM_CALL_FAILED;
		}
		return conf.gvm->find_domain(domain,ctx);
	}
	case CALL_CLEAN_ALL_CACHE:
	{
		dead_all_obj();
		return WHM_OK;
	}
	case CALL_CACHE_PREFETCH:	
	case CALL_CACHE_INFO:
	case CALL_CLEAN_CACHE:
		{
			KCacheInfo ci;
			memset(&ci,0,sizeof(ci));
			const char *url = uv->getx("url");
			if (url==NULL) {
				ctx->setStatus("url is missing");
				return WHM_CALL_FAILED;
			}
			char *buf = strdup(url);
			char *hot = buf;
			int result = 0;
			for (;;) {
				char *p = strstr(hot,", ");
				if (p) {
					*p = '\0';
				}
				char *u = hot;

				if (cmd == CALL_CACHE_INFO) {
					switch (*hot) {
					case '3':
					{
						result += get_cache_info(hot + 1, true, &ci);
						break;
					}
					case '0':
						u++;
					default:
						result += get_cache_info(u, false, &ci);
						break;
					}
				}
#ifdef ENABLE_SIMULATE_HTTP
				 else if (cmd == CALL_CACHE_PREFETCH) {
						if (cache_prefetch(hot)) {
							result++;
						}
					}
#endif
				else {
					switch (*hot) {
					case '1':
						{
							//正则，区分大小写
							KReg reg;
							if(reg.setModel(hot+1,0)){
								result += clean_cache(&reg,0);
							}
							break;
						}
							
					case '2':
						{
							//正则，不区分大小写
							KReg reg;
							if(reg.setModel(hot+1,PCRE_CASELESS)){
								result += clean_cache(&reg,0);
							}
							break;
						}					
					case '3':
						{
							//匹配前面部分
							result += clean_cache(hot+1,true);
							break;

						}						
					case '0':
						//精确匹配
						u++;
					default:
						//精确匹配
						result += clean_cache(u,false);
						break;
					}
				}
				if (p==NULL) {
					break;
				}
				hot = p+2;
			}
			free(buf);
			if (cmd==CALL_CACHE_INFO) {
				ctx->add("mem_size",ci.mem_size);
				ctx->add("disk_size",ci.disk_size);
			}
			ctx->add("count",result);
			return WHM_OK;
		}		
#ifdef ENABLE_VH_FLOW
	case CALL_DUMP_FLOW:
		{
			const char *prefix = uv->getx("prefix");
			bool revers = atoi(uv->get("revers").c_str())==1;
			int prefix_len = 0;
			if (prefix) {
				prefix_len = (int)strlen(prefix);
			}
			conf.gvm->dumpFlow(ctx,revers,prefix,prefix_len,atoi(uv->get("extend").c_str()));
			return WHM_OK;
		}
	case CALL_DUMP_LOAD:
	{
			const char *prefix = uv->getx("prefix");
			bool revers = atoi(uv->get("revers").c_str())==1;
			int prefix_len = 0;
			if (prefix) {
				prefix_len = (int)strlen(prefix);
			}
			conf.gvm->dumpLoad(ctx,revers,prefix,prefix_len);
			return WHM_OK;
	}
	case CALL_GET_LOAD:
		{
			KVirtualHost *vh = ctx->getVh();
			if (vh==NULL) {
				ctx->setStatus("cann't find vh");
				return WHM_CALL_FAILED;
			}
			bool reset = atoi(uv->get("reset").c_str())==1;
			ctx->add("speed",vh->get_speed(reset));
#ifdef ENABLE_VH_RS_LIMIT
			ctx->add("connect",vh->GetConnectionCount());
#endif
			return WHM_OK;
		}
	case CALL_GET_CONNECTION:
		{
			KConnectionInfoContext cn_ctx;
			cn_ctx.total_count = 0;
			cn_ctx.debug = 0;
			cn_ctx.vh = uv->getx("vh");
			cn_ctx.translate = false;
			kgl_iterator_sink(kgl_connection_iterator, (void*)&cn_ctx);
			ctx->add("count", cn_ctx.total_count);
			ctx->add("connection",cn_ctx.s.str().c_str(),true);
			return WHM_OK;
		}
#endif
#ifdef ENABLE_BLACK_LIST
	case CALL_BLACK_LIST:
		{
			KIpList *iplist = conf.gvm->vhs.blackList;
			if (uv->getx("vh")) {
				KVirtualHost *vh = ctx->getVh();
				
				if (vh==NULL) {
					ctx->setStatus("cann't find vh");
					return WHM_CALL_FAILED;
				}
				iplist = vh->blackList;
			}
			if (iplist==NULL) {
				ctx->setStatus("black list not support");
				return WHM_CALL_FAILED;
			}
			const char *a = uv->getx("a");
			if (a==NULL) {
				iplist->getBlackList(ctx);
				return WHM_OK;
			}
			if (strcmp(a, "check") == 0) {
				const char *ip = uv->getx("ip");
				if (ip == NULL) {
					ctx->setStatus("ip param is missing");
					return WHM_CALL_FAILED;
				}
				bool result = iplist->find(ip, 0, false);
				if (result) {
					ctx->add("hit", 1);
				} else {
					ctx->add("hit", (INT64)0);
				}
				return WHM_OK;
			}
			if (strcmp(a,"clear")==0) {
				iplist->clearBlackList();
			}
			return WHM_OK;
		}
	case CALL_VH_STAT:
		{
			return getVhStat(ctx);
		}
#ifdef ENABLE_SIMULATE_HTTP
	case CALL_REPORT_IP:
	{
		 const char *ips = uv->getx("ips");
		 if (ips==NULL) {
			ctx->setStatus("ips param is missing");
			return WHM_PARAM_ERROR;
		 }
		 add_report_ip(ips);
		 return WHM_OK;
	}
#endif
#endif//}}
	case CALL_CHECK_SSL:
		{
			KVirtualHost *vh = ctx->getVh();
			if (vh==NULL) {
				ctx->setStatus("cann't find vh");
				return WHM_CALL_FAILED;
			}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
			SSL_CTX *ssl_ctx = kgl_get_ssl_ctx(vh->ssl_ctx);
			//{{ent
#ifdef ENABLE_SVH_SSL
			const char *domain = uv->getx("domain");
			if (domain) {
				bool domain_finded = false;
				std::list<KSubVirtualHost *>::iterator it;
				for (it = vh->hosts.begin(); it != vh->hosts.end(); it++) {
					if ((*it)->MatchHost(domain)) {
						domain_finded = true;
						ssl_ctx = kgl_get_ssl_ctx((*it)->ssl_ctx);
						break;
					}
				}
				if (!domain_finded) {
					ctx->setStatus("domain not found");
					return WHM_CALL_FAILED;
				}
			}
#endif//}}
			ctx->add("ssl",ssl_ctx?1:0);
			if (ssl_ctx) {
				char *result = NULL;
				result = ssl_ctx_var_lookup(ssl_ctx,"NOTBEFORE");
				if (result) {
					ctx->add("not_before", result);
					ssl_var_free(result);
				}
				result = ssl_ctx_var_lookup(ssl_ctx, "NOTAFTER");
				if (result) {
					ctx->add("not_after", result);
					ssl_var_free(result);
				}
				result = ssl_ctx_var_lookup(ssl_ctx, "SUBJECT");
				if (result) {
					ctx->add("subject", result);
					ssl_var_free(result);
				}
			}
			return WHM_OK;
#endif
			ctx->setStatus("ssl sni not support");
			return WHM_CALL_FAILED;
		}
	case CALL_SERVER_INFO:
	{
		const char *name = uv->getx("name");
		if (name == NULL) {
			ctx->setStatus("name param is missing");
			return WHM_PARAM_ERROR;
		}
		KMultiAcserver *rd = server_container->refsMultiServer(name);
		std::stringstream s;
		if (rd) {
			rd->getNodeInfo(s);
			ctx->add("node", s.str().c_str());
			rd->release();
		}
		return WHM_OK;
	}
	case CALL_LIST_DSO:
	{
#ifdef ENABLE_KSAPI_FILTER
		if (conf.dem) {
			conf.dem->whm(ctx);
		}
#endif
		return WHM_OK;
	}
	case CALL_LIST_LISTEN:
	{
		conf.gvm->GetListenWhm(ctx);
		return WHM_OK;
	}
#ifdef ENABLE_LOG_DRILL
	case CALL_LOG_DRILL:
	{
		flush_log_drill();
		return WHM_OK;
	}
#endif

	case CALL_LIST_TABLE:	
		{
			//vh=vhname&access=response|request
			std::string err_msg;
			KVirtualHost *vh = ctx->getVh();
			if(vh==NULL &&  uv->getx("vh")){
				ctx->setStatus("cann't find such vh");
				return WHM_CALL_FAILED;
			}
			const char *access = uv->getx("access");
			if(access==NULL){
				ctx->setStatus("access must be set");
				return WHM_PARAM_ERROR;
			}
			int access_type = 0;
			if(strcasecmp(access,"response")==0){
				access_type = RESPONSE;
			}else if(strcasecmp(access,"request")==0) {
				access_type = REQUEST;
			} else {
				ctx->setStatus("access must be response or request");
				return WHM_PARAM_ERROR;
			}
			KSafeAccess maccess(kaccess[access_type]->add_ref());
			if (vh) {
#ifndef HTTP_PROXY				
				maccess = vh->access[access_type];
#endif
			}
			bool result = true;
			bool save_file = false;
			if (cmd==CALL_LIST_TABLE) {
				if (maccess) {
					maccess->listTable(ctx);
				}
			}
			if(!result){
				ctx->setStatus(err_msg.c_str());
				return WHM_CALL_FAILED;
			}
			return WHM_OK;
		}
	}
#if 0
	if (cmd == CALL_ADD_VH_INFO) {
		if(!vhd.addInfo(uv->attribute,errMsg)){
			ctx->setStatus(errMsg.c_str());
			return WHM_CALL_FAILED;
		}
		return WHM_OK;
	}
	if (cmd == CALL_DEL_VH_INFO) {
		if (!vhd.delInfo(uv->attribute,errMsg)){
			ctx->setStatus(errMsg.c_str());
			return WHM_CALL_FAILED;
		}
		return WHM_OK;
	}
	if(cmd == CALL_DEL_VH){
		return deleteVh(ctx);
	}
#endif
	if(cmd == CALL_LIST_VH){
		//ctx->setStatus(WHM_OK);
		std::list<std::string> vhs;
		if(cmd==CALL_LIST_VH){
			conf.gvm->getAllVh(
				vhs,
				uv->get("status")=="1",
				uv->get("onlydb")=="1"
				);
		}
		for(auto it=vhs.begin();it!=vhs.end();it++){
			ctx->add("name",(*it));
		}
		return WHM_OK;
	}
	if(cmd==CALL_INFO_DOMAIN){
		return getVhDomain(ctx);
	}
	if(cmd==CALL_INFO_VH){
		return getVhDetail(ctx);
	}
	if(cmd==CALL_LIST_INDEX){
		std::string name;
		KBaseVirtualHost *bvh = ctx->getVh();
		if(bvh==NULL){
			ctx->setStatus("no such vh");
			return WHM_CALL_FAILED;
		}
		bool result=false;		
		bvh->listIndex(ctx);
		result = true;
		
		if(result){
			return WHM_OK;
		}
		//ctx->setStatus(WHM_CALL_FAILED);
		return WHM_CALL_FAILED;
	}
	if(cmd==CALL_KILL_PROCESS){	
		if(!killProcess(ctx->getVh())){
			//ctx->setStatus(WHM_CALL_FAILED);
			return WHM_CALL_FAILED;	
		}
		return WHM_OK;
		//conf.gam->killProcess(uv->get("user"));
	}
	if (cmd==CALL_RELOAD) {
		do_config(false);
		wait_load_config_done();
		return WHM_OK;
	}
	if (cmd == CALL_RELOAD_VH_ACCESS) {
		KVirtualHost *vh = ctx->getVh();
#if 0
		if (vh == NULL) {
			ctx->setStatus("cann't find vh");
			return WHM_CALL_FAILED;
		}
		vh->access_file_loaded = false;
		vh->loadAccess(NULL);
#endif
		return WHM_OK;
	}
	if(cmd==CALL_RELOAD_VH){
		std::string name;
		bool initEvent = false;
		if (uv->get("init")=="1"||uv->get("init")=="true") {
			initEvent = true;
		}
		if (uv->get("name",name)) {
			if (!vhd.flushVirtualHost(name.c_str(),initEvent,ctx)) {
				return WHM_CALL_FAILED;
			}
		} else {
			const char *names = uv->getx("names");
			if (names && *names) {
				//有多个
				char *buf = strdup(names);
				char *hot = buf;
				for (;;) {
					char *p = strchr(hot,',');
					if (p) {
						*p = '\0';
					}
					if (*hot) {
						vhd.flushVirtualHost(hot,initEvent,ctx);
					}
					if (p==NULL) {
						break;
					}
					hot = p+1;
				}
				free(buf);
			} else {
				do_config(false);
			}
		}
		return WHM_OK;
	}
	if(cmd==CALL_CHANGE_ADMIN_PASSWORD){
		std::string errMsg;
		conf.admin_lock.Lock();
		if(!changeAdminPassword(uv,errMsg)){
			conf.admin_lock.Unlock();
			ctx->setStatus(errMsg.c_str());
			return WHM_CALL_FAILED;
		}
		conf.admin_lock.Unlock();
		return WHM_OK;
	}
	if(cmd==CALL_REBOOT){
		console_call_reboot();
		return WHM_OK;
	}
	if(cmd==CALL_WRITE_FILE){
		std::string file = uv->get("file");
		if(file.size()<=0){
			return WHM_CALL_FAILED;
		}
		std::string content = uv->get("content");
		std::string urlencode = uv->get("urlencode");
		if(isAbsolutePath(file.c_str())){
			#ifdef _WIN32
			if(file[0]=='/'){
				file = conf.diskName + file;
			}
			#endif	
		}else{
			file = conf.path + file;
		}
		int len = (int)content.size();
		char *buf = NULL;
		if(urlencode.size()>0 && urlencode=="1"){
			buf = strdup(content.c_str());
		} else {
			buf = b64decode((const unsigned char *)content.c_str(),&len);
			if(buf==NULL || len<=0){
				ctx->setStatus("cann't decode content");
				if(buf){
					xfree(buf);
				}
				return WHM_CALL_FAILED;
			}			
		}
		FILE *fp = fopen(file.c_str(),"wb");
		if(fp==NULL){
			ctx->setStatus("access denied");
			if(buf){
				xfree(buf);
			}
			return WHM_CALL_FAILED;
		}
		fwrite(buf,1,len,fp);
		fclose(fp);
		if (buf) {
			xfree(buf);
		}
		return WHM_OK;	
	}
	if(cmd==CALL_CHECK_VH_DB){
		if(vhd.check()){
			ctx->add("status","1");
		}else{
			ctx->add("status","0");
		}
		return WHM_OK;
	}
	return WHM_CALL_NOT_FOUND;
}
