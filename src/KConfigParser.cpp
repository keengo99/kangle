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
#include "KConfigParser.h"
#include "KXml.h"
#include "KAccess.h"
#include "KAcserverManager.h"
#include "KWriteBackManager.h"
#include "kmalloc.h"
#include "kthread.h"
#include "KRequestQueue.h"
#include "KHttpLib.h"
#include "kmd5.h"
#include "do_config.h"
#include "KListenConfigParser.h"
#include "kselector_manager.h"
#include "KDsoExtendManage.h"
using namespace std;
KConfigParser::KConfigParser() {

}

KConfigParser::~KConfigParser() {
}
bool KConfigParser::startElement(KXmlContext* context) {
#ifndef _WIN32
	if (context->qName == "run") {
		cconf->run_user = context->attribute["user"];
		cconf->run_group = context->attribute["group"];
	}
#endif
	if (context->qName == "admin") {
		cconf->admin_user = context->attribute["user"];
		cconf->passwd_crypt = parseCryptType(context->attribute["crypt"].c_str());
		cconf->auth_type = KHttpAuth::parseType(context->attribute["auth_type"].c_str());
		cconf->admin_passwd = context->attribute["password"];
		explode(context->attribute["admin_ips"].c_str(), '|', cconf->admin_ips);
		change_admin_password_crypt_type();
		return true;
	}
	if (context->qName == "gzip" || context->qName == "compress") {
		if (!context->attribute["only_compress_cache"].empty()) {
			cconf->only_compress_cache = atoi(context->attribute["only_compress_cache"].c_str());
		} else if (!context->attribute["only_gzip_cache"].empty()) {
			cconf->only_compress_cache = atoi(context->attribute["only_gzip_cache"].c_str());
		}
		if (!context->attribute["min_compress_length"].empty()) {
			cconf->min_compress_length = atoi(context->attribute["min_compress_length"].c_str());
		} else if (!context->attribute["min_gzip_length"].empty()) {
			cconf->min_compress_length = atoi(context->attribute["min_gzip_length"].c_str());
		}
		if (!context->attribute["gzip_level"].empty()) {
			cconf->gzip_level = atoi(context->attribute["gzip_level"].c_str());
		}
		if (!context->attribute["br_level"].empty()) {
			cconf->br_level = atoi(context->attribute["br_level"].c_str());
		}
		return true;
	}
	if (context->qName == "cache") {
		if (!context->attribute["default"].empty()) {
			cconf->default_cache = atoi(context->attribute["default"].c_str());
		}
		if (!context->attribute["max_cache_size"].empty()) {
			cconf->max_cache_size = (unsigned)get_size(context->attribute["max_cache_size"].c_str());
		}
		if (context->attribute["memory"].size() > 0) {
			cconf->mem_cache = get_size(context->attribute["memory"].c_str());
		}
		if (context->attribute["refresh_time"].size() > 0) {
			cconf->refresh_time = atoi(context->attribute["refresh_time"].c_str());
		}
#ifdef ENABLE_DISK_CACHE
		if (!context->attribute["max_bigobj_size"].empty()) {
			cconf->max_bigobj_size = get_size(context->attribute["max_bigobj_size"].c_str());
		}
		if (context->attribute["disk"].size() > 0) {
			cconf->disk_cache = get_radio_size(context->attribute["disk"].c_str(), cconf->disk_cache_is_radio);
		}
		if (context->attribute["disk_dir"].size() > 0) {
			SAFE_STRCPY(cconf->disk_cache_dir2, context->attribute["disk_dir"].c_str());
		}
		if (context->attribute["disk_work_time"].size() > 0) {
			SAFE_STRCPY(cconf->disk_work_time, context->attribute["disk_work_time"].c_str());
		}
#ifdef ENABLE_BIG_OBJECT_206
		if (!context->attribute["cache_part"].empty()) {
			cconf->cache_part = context->attribute["cache_part"] == "1";
		}
#endif
#endif
		return true;
	}
	if (context->qName == "connect") {
		cconf->max_per_ip = atoi(context->attribute["max_per_ip"].c_str());
		cconf->max = atoi(context->attribute["max"].c_str());
		cconf->per_ip_deny = atoi(context->attribute["per_ip_deny"].c_str());
		return true;
	}

	if (context->qName == "cdnbest") {
		std::map<string, string>::iterator it;
		it = context->attribute.find("error");
		if (it != context->attribute.end()) {
			SAFE_STRCPY(cconf->error_url, (*it).second.c_str());
		}
		return true;
	}
	if (context->qName == "log") {
		if (context->attribute["level"].size() > 0) {
			cconf->log_level = atoi(context->attribute["level"].c_str());
		}
		SAFE_STRCPY(cconf->log_rotate, context->attribute["rotate_time"].c_str());
		if (context->attribute["rotate_size"].size() > 0) {
			cconf->log_rotate_size = get_size(context->attribute["rotate_size"].c_str());
		}
		cconf->logs_day = atoi(context->attribute["logs_day"].c_str());
		if (context->attribute["logs_size"].size() > 0) {
			cconf->logs_size = get_size(context->attribute["logs_size"].c_str());
		}
		if (context->attribute["error_rotate_size"].size() > 0) {
			cconf->error_rotate_size = get_size(context->attribute["error_rotate_size"].c_str());
		}
		if (!context->attribute["radio"].empty()) {
			cconf->log_radio = atoi(context->attribute["radio"].c_str());
		}
		cconf->log_handle = context->attribute["log_handle"] == "1";
		cconf->log_sub_request = context->attribute["log_sub_request"] == "1";
		return true;
	}
	return false;

}

bool KConfigParser::startCharacter(KXmlContext* context, char* character, int len) {
	if (context->path == "config/admin") {
		if (context->qName == "allowip") {
			cconf->admin_ips.push_back(character);
			return true;
		}
	}
	if (context->path == "config/cache") {
		if (context->qName == "memory") {
			cconf->mem_cache = get_size(character);
			return true;
		}
		if (context->qName == "refresh_time") {
			cconf->refresh_time = atoi(character);
			return true;
		}
#ifdef ENABLE_DISK_CACHE
		if (context->qName == "disk") {
			cconf->disk_cache = get_size(character);
		}
		if (context->qName == "disk_dir") {
			SAFE_STRCPY(cconf->disk_cache_dir2, character);
		}
		if (context->qName == "disk_work_time") {
			SAFE_STRCPY(cconf->disk_work_time, character);
		}
#endif
	}
	if (context->path == "config") {
#ifdef MALLOCDEBUG
		if (context->qName == "mallocdebug") {
			if (atoi(character) == 0) {
				cconf->mallocdebug = false;
			} else {
				cconf->mallocdebug = true;
			}
		}
#endif
		if (context->qName == "http2https_code") {
			cconf->http2https_code = atoi(character);
			return true;
		}

		if (context->qName == "max_connect_info") {
			cconf->max_connect_info = atoi(character);
			return true;
		}
#ifdef KSOCKET_UNIX	
		if (context->qName == "unix_socket") {
			if (strcmp(character, "1") == 0 || strcmp(character, "on") == 0) {
				cconf->unix_socket = true;
			} else {
				cconf->unix_socket = false;
			}
		}
#endif
		if (context->qName == "path_info") {
			if (strcmp(character, "1") == 0 || strcmp(character, "on") == 0) {
				cconf->path_info = true;
			} else {
				cconf->path_info = false;
			}
		}
		if (context->qName == "min_free_thread") {
			cconf->min_free_thread = atoi(character);
		}
		//{{ent
#ifdef ENABLE_ADPP
		if (context->qName == "process_cpu_usage") {
			cconf->process_cpu_usage = atoi(character);
		}
#endif
		if (context->qName == "apache_config_file") {
			cconf->apache_config_file = character;
		}
		//}}
		if (context->qName == "access_log") {
			SAFE_STRCPY(cconf->access_log, character);
		}
		if (context->qName == "access_log_handle") {
			SAFE_STRCPY(cconf->logHandle, character);
		}
		if (context->qName == "log_handle_concurrent") {
			cconf->maxLogHandle = atoi(character);
			return true;
		}
		if (context->qName == "log_event_id") {
			cconf->log_event_id = atoi(character);
			return true;
		}

#ifdef ENABLE_LOG_DRILL
		if (context->qName == "log_drill") {
			cconf->log_drill = atoi(character);
			if (cconf->log_drill > 65536) {
				cconf->log_drill = 65536;
			}
			if (cconf->log_drill < 0) {
				cconf->log_drill = 0;
			}
		}
#endif
		if (context->qName == "server_software") {
			SAFE_STRCPY(cconf->server_software, character);
		}
		if (context->qName == "cookie_stick_name") {
			SAFE_STRCPY(cconf->cookie_stick_name, character);
		}
		if (context->qName == "hostname") {
			cconf->setHostname(character);
		}
#ifdef ENABLE_TF_EXCHANGE
		if (context->qName == "max_post_size") {
			cconf->max_post_size = get_size(character);
		}
#endif
		//{{ent
#ifdef ENABLE_VH_FLOW
		if (context->qName == "flush_flow_time") {
			cconf->flush_flow_time = atoi(character);
		}
#endif
		if (context->qName == "upstream_sign") {
			SAFE_STRCPY(cconf->upstream_sign, character);
			cconf->upstream_sign_len = (int)strlen(cconf->upstream_sign);
			return true;
		}
#ifdef ENABLE_BLACK_LIST
		if (context->qName == "bl_time") {
			int t = atoi(character);
			if (t > 0) {
				cconf->bl_time = t;
			}
		}
		if (context->qName == "block_ip_cmd") {
			SAFE_STRCPY(cconf->block_ip_cmd, character);
		}
		if (context->qName == "unblock_ip_cmd") {
			SAFE_STRCPY(cconf->unblock_ip_cmd, character);
		}
		if (context->qName == "flush_ip_cmd") {
			SAFE_STRCPY(cconf->flush_ip_cmd, character);
		}
		if (context->qName == "report_url") {
			SAFE_STRCPY(cconf->report_url, character);
		}
#endif
		//}}
		if (context->qName == "read_hup") {
			cconf->read_hup = (atoi(character) == 1);
			return true;
		}
		if (context->qName == "io_buffer") {
			cconf->io_buffer = (unsigned)get_size(character);
			cconf->io_buffer = kgl_align(cconf->io_buffer, 1024);
			return true;
		}
		if (context->qName == "fiber_stack_size") {
			int fiber_stack_size = (int)get_size(character);
			fiber_stack_size = kgl_align(fiber_stack_size, 4096);
			if (fiber_stack_size > 0) {
				cconf->fiber_stack_size = fiber_stack_size;
			}
		}
		if (context->qName == "worker_io") {
			cconf->worker_io = atoi(character);
			return true;
		}
		if (context->qName == "max_io") {
			cconf->max_io = atoi(character);
			return true;
		}
		if (context->qName == "worker_dns") {
			cconf->worker_dns = atoi(character);
			return true;
		}
		if (context->qName == "ssl_client_protocols") {
			cconf->ssl_client_protocols = character;
		}
		if (context->qName == "ssl_client_chiper") {
			cconf->ssl_client_chiper = character;
		}
		if (context->qName == "ca_path") {
			cconf->ca_path = character;
		}
	}
	return false;

}

bool KConfigParser::endElement(KXmlContext* context) {

	return false;
}
void KConfigParser::startXml(const std::string& encoding) {

}
void KConfigParser::endXml(bool result) {
}


void KMainConfigListen::on_event(kconfig::KConfigTree* tree, KXmlNode* xml, kconfig::KConfigEventType ev) {
	switch (ev) {
	case kconfig::KConfigEventType::New:
	case kconfig::KConfigEventType::Update:
		if (xml->is_tag(_KS("lang"))) {
			SAFE_STRCPY(conf.lang, xml->get_text());
			return;
		}
		if (xml->is_tag(_KS("keep_alive_count"))) {
			conf.keep_alive_count = atoi(xml->get_text());
			return;
		}
		if (xml->is_tag(_KS("timeout"))) {
			auto it = xml->attributes.find("rw");
			if (it != xml->attributes.end()) {
				conf.set_time_out(atoi((*it).second.c_str()));
				conf.set_connect_time_out(atoi(xml->attributes["connect"].c_str()));
			} else {
				conf.set_time_out(atoi(xml->get_text()));
			}
			return;
		}
		if (xml->is_tag(_KS("connect_timeout"))) {
			conf.set_connect_time_out(atoi(xml->get_text()));
			return;
		}
		if (xml->is_tag(_KS("auth_delay"))) {
			conf.auth_delay = atoi(xml->get_text());
			return;
		}
		break;
	default:
		break;
	}
}