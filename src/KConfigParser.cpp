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
#include "KDefer.h"
#include "KDsoExtendManage.h"
#include "KLogHandle.h"
#include "KCache.h"
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
	return false;

}

bool KConfigParser::startCharacter(KXmlContext* context, char* character, int len) {
	if (context->path == "config") {
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
void on_cache_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (ev == nullptr || ev->type == kconfig::EvRemove) {
		//default
		conf.default_cache = 1;
		conf.max_cache_size = (unsigned)get_size("10M");
		conf.mem_cache = get_size("1G");
		conf.disk_cache = 0;
		cache.init(kconfig::is_first());
		return;
	}
	auto xml = ev->get_xml();

	conf.default_cache = xml->get_first()->attributes.get_int("default", 1);
	conf.max_cache_size = (unsigned)get_size(xml->get_first()->attributes.get_string("max_cache_size", "10M"));
	conf.mem_cache = get_size(xml->get_first()->attributes.get_string("memory", "1G"));
	conf.refresh_time = xml->get_first()->attributes.get_int("refresh_time", 30);
#ifdef ENABLE_DISK_CACHE
	conf.max_bigobj_size = get_size(xml->get_first()->attributes("memory", "1G"));
	conf.disk_cache = get_radio_size(xml->get_first()->attributes("disk", "0"), conf.disk_cache_is_radio);
	SAFE_STRCPY(conf.disk_cache_dir2, xml->get_first()->attributes("disk_dir", ""));
	if (!kconfig::is_first()) {
		if (strcasecmp(conf.disk_cache_dir2, conf.disk_cache_dir) != 0) {
			kconfig::set_need_reboot();
		}
	} else {
		//Éú³Édisk_cache_dir
		if (*conf.disk_cache_dir2) {
			string disk_cache_dir = conf.disk_cache_dir2;
			pathEnd(disk_cache_dir);
			SAFE_STRCPY(conf.disk_cache_dir, disk_cache_dir.c_str());
		}
	}
	SAFE_STRCPY(conf.disk_work_time, xml->get_first()->attributes("disk_work_time", ""));
	if (*conf.disk_work_time) {
		conf.diskWorkTime.set(conf.disk_work_time);
	} else {
		conf.diskWorkTime.set(NULL);
	}
#ifdef ENABLE_BIG_OBJECT_206
	conf.cache_part = xml->get_first()->attributes.get_int("cache_part", 1) == 1;
#endif
#endif
	cache.init(kconfig::is_first());

}
void on_log_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (ev == nullptr || ev->type == kconfig::EvRemove) {
		//default
		conf.log_level = 2;
		*conf.log_rotate = '\0';
		conf.logs_size = get_size("1G");
		conf.log_rotate_size = get_size("100M");
		conf.error_rotate_size = get_size("100M");
		klog_start();
		::logHandle.setLogHandle(conf.logHandle);
		return;
	}
	auto attr = ev->get_xml()->attributes();
	switch (ev->type) {
	case kconfig::EvNew:
	case kconfig::EvUpdate:
		conf.log_level = attr.get_int("level", 2);
		SAFE_STRCPY(conf.log_rotate, attr["rotate_time"].c_str());
		conf.log_rotate_size = get_size(attr.get_string("rotate_size", "100M"));
		conf.logs_day = attr.get_int("logs_day");
		conf.logs_size = get_size(attr.get_string("logs_size", "1G"));
		conf.error_rotate_size = get_size(attr.get_string("error_rotate_size", "100M"));
		conf.log_radio = attr.get_int("radio");
		conf.log_handle = attr.get_int("log_handle") == 1;
		klog_start();
		::logHandle.setLogHandle(conf.logHandle);
		break;
	default:
		break;
	}
}
void on_admin_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	conf.admin_lock.Lock();
	defer(conf.admin_lock.Unlock());
	auto attr = ev->get_xml()->attributes();
	switch (ev->type) {
	case kconfig::EvNew:
	case kconfig::EvUpdate:
		conf.admin_user = attr["user"];
		conf.passwd_crypt = parseCryptType(attr["crypt"].c_str());
		conf.auth_type = KHttpAuth::parseType(attr["auth_type"].c_str());
		conf.admin_passwd = attr["password"];
		explode(attr["admin_ips"].c_str(), '|', conf.admin_ips);
		//change_admin_password_crypt_type();
		break;
	default:
		break;
	}
}
void on_ssl_client_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	auto attr = ev->get_xml()->attributes();
	switch (ev->type) {
	case kconfig::EvNew:
	case kconfig::EvUpdate:
		khttp_server_set_ssl_config(attr.get_string("ca_path"), attr.get_string("chiper"), attr.get_string("protocols"));
		break;
	case kconfig::EvRemove:
		khttp_server_set_ssl_config("", "", "");
		break;
	default:
		break;
	}
}
void on_main_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	auto xml = ev->get_xml();
	KBIT_CLR(ev->type, kconfig::EvSubDir);
	switch (ev->type) {
	case kconfig::EvNew:
	case kconfig::EvUpdate:
		if (xml->is_tag(_KS("lang"))) {
			SAFE_STRCPY(conf.lang, xml->get_text());
			return;
		}
		if (xml->is_tag(_KS("keep_alive_count"))) {
			conf.keep_alive_count = atoi(xml->get_text());
			return;
		}
		if (xml->is_tag(_KS("timeout"))) {
			auto attributes = xml->attributes();
			auto it = attributes.find("rw");
			if (it != attributes.end()) {
				conf.set_time_out(atoi((*it).second.c_str()));
				conf.set_connect_time_out(atoi(xml->attributes()["connect"].c_str()));
			} else {
				conf.set_time_out(atoi(xml->get_text()));
			}
			selector_manager_set_timeout(conf.connect_time_out, conf.time_out);
			http_config.time_out = conf.time_out;
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
		if (xml->is_tag(_KS("access_log"))) {
			SAFE_STRCPY(conf.access_log, xml->get_text());
			return;
		}
		if (xml->is_tag(_KS("access_log_handle"))) {
			SAFE_STRCPY(conf.logHandle, xml->get_text());
			return;
		}
		if (xml->is_tag(_KS("log_handle_concurrent"))) {
			conf.maxLogHandle = atoi(xml->get_text());
			return;
		}
		if (xml->is_tag(_KS("log_event_id"))) {
			conf.log_event_id = atoi(xml->get_text());
			return;
		}
		if (xml->is_tag(_KS("server_software"))) {
			SAFE_STRCPY(conf.server_software, xml->get_text());
			return;
		}
		if (xml->is_tag(_KS("cookie_stick_name"))) {
			SAFE_STRCPY(conf.cookie_stick_name, xml->get_text());
			return;
		}
		if (xml->is_tag(_KS("hostname"))) {
			SAFE_STRCPY(conf.hostname, xml->get_text());
			return;
		}
#ifdef ENABLE_LOG_DRILL
		if (xml->is_tag(_KS("log_drill"))) {
			conf.log_drill = atoi(xml->get_text());
			if (conf.log_drill > 65536) {
				conf.log_drill = 65536;
			}
			if (conf.log_drill < 0) {
				conf.log_drill = 0;
			}
			return;
		}
#endif
		break;
	default:
		break;
	}
}