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
#include "KVirtualHostDatabase.h"
#include "KLogHandle.h"
#include "KCache.h"
#include "KHttpServer.h"

using namespace std;
void on_firewall_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
#ifdef ENABLE_BLACK_LIST
	if (ev == nullptr || ev->type == kconfig::EvRemove) {
		conf.bl_time = 0;
		conf.wl_time = 1800;
		*conf.block_ip_cmd = '\0';
		*conf.unblock_ip_cmd = '\0';
		*conf.flush_ip_cmd = '\0';
	} else {
		auto attr = ev->get_xml()->attributes();
		conf.bl_time = attr.get_int("bl_time");
		conf.wl_time = attr.get_int("wl_time", 1800);
		SAFE_STRCPY(conf.block_ip_cmd, attr("block_ip_cmd"));
		SAFE_STRCPY(conf.unblock_ip_cmd, attr("unblock_ip_cmd"));
		SAFE_STRCPY(conf.flush_ip_cmd, attr("flush_ip_cmd"));
		SAFE_STRCPY(conf.report_url, attr("report_url"));
	}
	if (kconfig::is_first() && *conf.flush_ip_cmd) {
		run_fw_cmd(conf.flush_ip_cmd, NULL);
	}
	conf.gvm->vhs.blackList->setRunFwCmd(*conf.block_ip_cmd != '\0');
	conf.gvm->vhs.blackList->setReportIp(*conf.report_url != '\0');
#endif
}
void on_compress_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (ev == nullptr || ev->type == kconfig::EvRemove) {
		conf.only_compress_cache = 0;
		conf.min_compress_length = 512;
		conf.gzip_level = 5;
		conf.br_level = 5;
	} else {
		auto attr = ev->get_xml()->attributes();
		conf.only_compress_cache = attr.get_int("only_cache", 0);
		conf.min_compress_length = attr.get_int("min_length", 512);
		conf.gzip_level = attr.get_int("gzip_level", 5);
		conf.br_level = attr.get_int("br_level", 5);
	}
}
void on_connect_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (ev == nullptr || ev->type == kconfig::EvRemove) {
		conf.max = 0;
		conf.max_per_ip = 0;
		conf.keep_alive_count = 0;
		conf.per_ip_deny = 0;
	} else {
		auto attr = ev->get_xml()->attributes();
		conf.max = attr.get_int("max");
		conf.max_per_ip = attr.get_int("max_per_ip");
		conf.keep_alive_count = attr.get_int("max_keep_alive");
		conf.per_ip_deny = attr.get_int("per_ip_deny");
	}
}
void on_run_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (ev == nullptr || ev->type == kconfig::EvRemove) {
		conf.run_user = "";
		conf.run_group = "";
	} else {
		auto attr = ev->get_xml()->attributes();
		conf.run_user = attr["user"];
		conf.run_group = attr["group"];
	}
}
void on_fiber_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (ev == nullptr || ev->type == kconfig::EvRemove) {
		conf.fiber_stack_size = 0;
		http_config.fiber_stack_size = conf.fiber_stack_size;
		return;
	}
	auto attr = ev->get_xml()->attributes();
	conf.fiber_stack_size = get_size(attr.get_string("stack_size", ""));
	if (conf.fiber_stack_size > 0) {
		conf.fiber_stack_size = kgl_align(conf.fiber_stack_size, 4096);
	}
	http_config.fiber_stack_size = conf.fiber_stack_size;
}
void on_dns_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (ev == nullptr || ev->type == kconfig::EvRemove) {
		conf.worker_dns = 8;
	} else {
		auto attr = ev->get_xml()->attributes();
		conf.worker_dns = attr.get_int("worker", 8);
	}
	if (conf.dnsWorker == NULL) {
		conf.dnsWorker = kasync_worker_init(conf.worker_dns, 512);
	} else {
		kasync_worker_set(conf.dnsWorker, conf.worker_dns, 512);
	}
}
void on_io_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (ev == nullptr || ev->type == kconfig::EvRemove) {
		conf.worker_io = 2;
		conf.max_io = 0;
		conf.io_buffer = 262144;
	} else {
		auto attr = ev->get_xml()->attributes();
		conf.worker_io = attr.get_int("worker", 2);
		conf.max_io = attr.get_int("max", 0);
		conf.io_buffer = attr.get_int("buffer", 262144);
	}
	conf.io_buffer = kgl_align(conf.io_buffer, 1024);
	if (conf.ioWorker == NULL) {
		conf.ioWorker = kasync_worker_init(conf.worker_io, conf.max_io);
	} else {
		kasync_worker_set(conf.ioWorker, conf.worker_io, conf.max_io);
	}
}
void on_cache_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (ev == nullptr || ev->type == kconfig::EvRemove) {
		//default
		conf.default_cache = 1;
		conf.max_cache_size = (unsigned)get_size("10M");
		conf.mem_cache = get_size("1G");
#ifdef ENABLE_DISK_CACHE
		conf.disk_cache = 0;
		*conf.disk_work_time = '\0';
		conf.diskWorkTime.set(NULL);
#ifdef ENABLE_BIG_OBJECT_206
		conf.cache_part = true;
#endif
#endif
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
void on_log_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (ev == nullptr || ev->type == kconfig::EvRemove) {
		//default
		conf.log_level = 2;
		*conf.log_rotate = '\0';
		conf.logs_size = get_size("1G");
		conf.log_rotate_size = get_size("100M");
		conf.error_rotate_size = get_size("100M");
		SAFE_STRCPY(conf.access_log, "access.log");
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
		if (*conf.access_log == '\0') {
			SAFE_STRCPY(conf.access_log, attr.get_string("access", "access.log"));
		}
		klog_start();
		::logHandle.setLogHandle(conf.logHandle);
		break;
	default:
		break;
	}
}
void on_admin_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
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
void on_ssl_client_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
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
void on_main_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
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
			parse_server_software();
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
		if (xml->is_tag(_KS("cdnbest"))) {
			auto attr = xml->attributes();
			SAFE_STRCPY(conf.error_url, attr("error"));
			return;
		}
		if (xml->is_tag(_KS("http2https_code"))) {
			conf.http2https_code = atoi(xml->get_text());
			return;
		}
		if (xml->is_tag(_KS("max_connect_info"))) {
			conf.max_connect_info = atoi(xml->get_text());
			return;
		}
#ifdef KSOCKET_UNIX	
		if (xml->is_tag(_KS("unix_socket"))) {
			conf.unix_socket = (atoi(xml->get_text()) == 1 || strcmp(xml->get_text(), "on") == 0);
			return;
		}
#endif
		if (xml->is_tag(_KS("path_info"))) {
			conf.path_info = (atoi(xml->get_text()) == 1 || strcmp(xml->get_text(), "on") == 0);
			return;
		}
		if (xml->is_tag(_KS("min_free_thread"))) {
			conf.min_free_thread = atoi(xml->get_text());
			return;
		}
#ifdef ENABLE_ADPP
		if (xml->is_tag(_KS("process_cpu_usage"))) {
			conf.process_cpu_usage = atoi(xml->get_text());
			return;
		}
#endif
#ifdef ENABLE_TF_EXCHANGE
		if (xml->is_tag(_KS("max_post_size"))) {
			conf.max_post_size = get_size(xml->get_text());
			return;
		}
#endif
#ifdef ENABLE_VH_FLOW
		if (xml->is_tag(_KS("flush_flow_time"))) {
			conf.flush_flow_time = atoi(xml->get_text());
			return;
		}
#endif
		if (xml->is_tag(_KS("upstream_sign"))) {
			SAFE_STRCPY(conf.upstream_sign, xml->get_text());
			conf.upstream_sign_len = (int)strlen(conf.upstream_sign);
			return;
		}
		if (xml->is_tag(_KS("read_hup"))) {
			conf.read_hup = (atoi(xml->get_text()) == 1);
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
	case kconfig::EvRemove:
		if (xml->is_tag(_KS("cdnbest"))) {
			*conf.error_url = '\0';
			return;
		}
		break;
	default:
		break;
	}
}