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
#include "KConfigBuilder.h"
#include "KAccess.h"
#include "KAcserver.h"
#include "KWriteBack.h"
#include "KAcserverManager.h"
#include "KWriteBackManager.h"
#include "KRequestQueue.h"
#include "kselector_manager.h"
#include<sstream>
#include "kmalloc.h"
using namespace std;
KConfigBuilder::KConfigBuilder() {
}

KConfigBuilder::~KConfigBuilder() {
}
bool KConfigBuilder::saveConfig() {
	KConfigBuilder builder;
#ifdef KANGLE_ETC_DIR
	string configFile = KANGLE_ETC_DIR;
#else
	string configFile = conf.path + "/etc";
#endif
	configFile += CONFIG_FILE;
	string tmpfile = configFile + ".tmp";
	string lstfile = configFile + ".lst";
	KFile fp;
	if(!fp.open(tmpfile.c_str(),fileWrite)){
		fprintf(stderr, "cann't open configfile[%s] for write\n",
				tmpfile.c_str());
		return false;
	}
	stringstream s;
	builder.build(s);
	s << "\r\n" << CONFIG_FILE_SIGN;
	bool result = false;
	if ((int)s.str().size() == fp.write(s.str().c_str(), (int)s.str().size())) {
		result = true;
	}
	fp.close();
	if (!result) {
		fprintf(stderr,"cann't write sign string\n");
		return false;
	}
	unlink(lstfile.c_str());
	rename(configFile.c_str(),lstfile.c_str());
	rename(tmpfile.c_str(),configFile.c_str());
	if (conf.mergeFiles.size()>0) {
		//remove the merge config files.
		std::list<std::string>::iterator it;
		for(it=conf.mergeFiles.begin();it!=conf.mergeFiles.end();it++){
			unlink((*it).c_str());
		}
		conf.mergeFiles.clear();
		std::string errMsg;
		conf.gvm->saveConfig(errMsg);
	}
	//∞—vh.xml÷ÿ√¸√˚
	string file = conf.path;
	file += VH_CONFIG_FILE;
	string oldfile = file + ".old";
	rename(file.c_str(),oldfile.c_str());
	return true;
}
void KConfigBuilder::build(std::stringstream &s) {
	unsigned i;
	s << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
	s << "<config>\n";
#ifdef MALLOCDEBUG
	if(!conf.mallocdebug){
		s << "\t<mallocdebug>0</mallocdebug>\n";
	}
#endif
	if(conf.select_count>0){
		s << "\t<worker_thread>" << conf.select_count << "</worker_thread>\n";
	}
	s << "\t<!--listen start-->\n";
	for (i = 0; i < conf.service.size(); i++) {
		if(conf.service[i]->ext){
			continue;
		}
		s << "\t<listen ";
		/*
		if (conf.service[i]->name.size()>0) {
			s << "name='" << conf.service[i]->name << "' ";
		}
		*/
		s << "ip='" << conf.service[i]->ip << "' ";
		s << "port='" << conf.service[i]->port << "' type='"
				<< getWorkModelName(conf.service[i]->model) << "' ";		
#ifdef KSOCKET_SSL
		if(KBIT_TEST(conf.service[i]->model,WORK_MODEL_SSL)){
			if (!conf.service[i]->cert_file.empty()) {
				s << "certificate='" << conf.service[i]->cert_file << "' ";
			}
			if (!conf.service[i]->key_file.empty()) {
				s << "certificate_key='" << conf.service[i]->key_file << "' ";
			}
			if (!conf.service[i]->cipher.empty()) {
				s << "cipher='" << conf.service[i]->cipher << "' ";
			}
			if (!conf.service[i]->protocols.empty()) {
				s << "protocols='" << conf.service[i]->protocols << "' ";
			}
#ifdef ENABLE_HTTP2
			s << "http2='" << (conf.service[i]->http2 ?"1":"0") << "' ";
#endif
			s << "early_data='" << (conf.service[i]->early_data ? "1" : "0") << "' ";
		}
#endif
		s << "/>\n";
	}
	s << "\t<!--listen end-->\n";
#ifndef _WIN32
	if (conf.run_user.size()>0) {
		s << "\t<run user='" << conf.run_user << "'";
		if(conf.run_group.size()>0) {
			s << " group='" << conf.run_group << "'";
		}
		s << "/>\n";
	}
#endif
	s << "\t<lang>" << conf.lang << "</lang>\n";
	s << "\t<keep_alive_count>" << conf.keep_alive_count << "</keep_alive_count>\n";
	s << "\t<timeout>" << conf.time_out << "</timeout>\n";
	s << "\t<connect_timeout>" << conf.connect_time_out << "</connect_timeout>\n";
	s << "\t<min_free_thread>" << conf.min_free_thread << "</min_free_thread>\n";	
//{{ent
#ifdef ENABLE_ADPP
	s << "\t<process_cpu_usage>" << conf.process_cpu_usage << "</process_cpu_usage>\n";
#endif
//}}
	s << "\t<admin user='" << conf.admin_user << "' password='"
			<< conf.admin_passwd << "' crypt='" << buildCryptType(
			conf.passwd_crypt) << "' auth_type='" << KHttpAuth::buildType(
			conf.auth_type) << "' admin_ips='";
	for (i = 0; i < conf.admin_ips.size(); i++) {
		if (i>0) {
			s << "|";
		}
		s << conf.admin_ips[i];
	}
	s << "'/>\n";
	s << "\t<compress only_compress_cache='" << conf.only_compress_cache << "' "
		<< "min_compress_length='" << conf.min_compress_length << "' "
		<< "gzip_level = '" << conf.gzip_level << "' "
		<< "br_level='" << conf.br_level << "'"
		<< "/>\n";
	s << "\t<cache default='" << conf.default_cache << "' max_cache_size='" << get_size(conf.max_cache_size) << "'";
	s << " memory='" <<  get_size(conf.mem_cache) << "'";
#ifdef ENABLE_DISK_CACHE
	s << " max_bigobj_size='" << get_size(conf.max_bigobj_size) << "'";
	s << " disk='" << get_size(conf.disk_cache);
	if (conf.disk_cache_is_radio) {
		s << "%";
	}
	s << "'";
	if (*conf.disk_cache_dir2) {
		s << " disk_dir='" << KXml::param(conf.disk_cache_dir2) << "'";
	}
	if (*conf.disk_work_time) {
		s << " disk_work_time='" << conf.disk_work_time << "'";
	}
#ifdef ENABLE_BIG_OBJECT_206
	if (conf.cache_part) {
		s << " cache_part='1'";
	}
#endif
#endif
	s << " refresh_time='" << conf.refresh_time << "'";
	s << "/>\n";
	s << "\t<connect max_per_ip='" << conf.max_per_ip << "' max='" << conf.max << "' ";
#ifndef _WIN32
	//s << "stack_size='" << conf.stack_size << "'";
#endif
	if (conf.per_ip_deny>0) {
		s << " per_ip_deny='1'";
	}
	s << ">\n";
#if 0
	ipLock.Lock();
	KPerIpConnect *per_ip = conf.per_ip_head;
	while (per_ip) {
		s << "\t\t<per_ip src='";
		char ips[MAXIPLEN];
		ksocket_ipaddr_ip(&per_ip->src.addr,ips,sizeof(ips));
		s << ips;
		if (per_ip->src.mask_num > 0) {
			s << "/" << (int) per_ip->src.mask_num;
		}
		s << "' max='" ;
		if (per_ip->deny) {
			s << "deny";
		} else {
			s << per_ip->max;
		}
		s << "'/>\n";
		per_ip = per_ip->next;		
	}
	ipLock.Unlock();
#endif
	s << "\t</connect>\n";
#ifdef ENABLE_TF_EXCHANGE
	//s << "\t<tempfile>" << conf.tmpfile << "</tempfile>\n";
	s << "\t<max_post_size>" << get_size(conf.max_post_size) << "</max_post_size>\n";
#endif
	//s << "\t<async_io>" << (conf.async_io?1:0) << "</async_io>\n";
#ifdef ENABLE_REQUEST_QUEUE
	unsigned max_worker = globalRequestQueue.getMaxWorker();
	if(max_worker>0){
		s << "\t<request_queue max_worker='" << max_worker << "' max_queue='" << globalRequestQueue.getMaxQueue() << "'/>\n";
	}
#endif
//{{ent
#ifdef ENABLE_BLACK_LIST
	if (conf.bl_time>0) {
           s << "\t<bl_time>" << conf.bl_time << "</bl_time>\n";
    }
	if (*conf.block_ip_cmd) {
		s << "\t<block_ip_cmd>" << conf.block_ip_cmd << "</block_ip_cmd>\n";
	}
	if (*conf.unblock_ip_cmd) {
		s << "\t<unblock_ip_cmd>" << conf.unblock_ip_cmd << "</unblock_ip_cmd>\n";
	}
	if (*conf.flush_ip_cmd) {
		s << "\t<flush_ip_cmd>" << conf.flush_ip_cmd << "</flush_ip_cmd>\n";
	}
#endif
	if(conf.apache_config_file.size()>0){
		s << "\t<apache_config_file>" << conf.apache_config_file << "</apache_config_file>\n";
	}
#ifdef ENABLE_VH_FLOW
	if (conf.flush_flow_time>0) {
		s << "\t<flush_flow_time>" << conf.flush_flow_time << "</flush_flow_time>\n";
	}
#endif
//}}
#ifdef KSOCKET_UNIX	
	if(conf.unix_socket){
		 s << "\t<unix_socket>1</unix_socket>\n";
	}
#endif
	s << "\t<path_info>" << (conf.path_info?1:0) << "</path_info>\n";
	s << "\t<access_log>" << conf.access_log << "</access_log>\n";
	if (*conf.logHandle) {
		s << "\t<access_log_handle>";
		s << CDATA_START << conf.logHandle << CDATA_END;
		s <<  "</access_log_handle>\n";
	}
	if(conf.maxLogHandle>0){
		s << "\t<log_handle_concurrent>" << conf.maxLogHandle << "</log_handle_concurrent>\n";
	}
	s << "\t<log level='" << conf.log_level << "'";
	if (*conf.log_rotate) {
		s << " rotate_time='" << conf.log_rotate << "'";
	}
	if (conf.log_rotate_size > 0) {
		s << " rotate_size='" << get_size(conf.log_rotate_size) << "'";
	}
	if (conf.error_rotate_size > 0) {
		s << " error_rotate_size='" << get_size(conf.error_rotate_size) << "'";
	}
	if (conf.logs_day>0) {
		s << " logs_day='" << conf.logs_day << "'";
	}
	if(conf.logs_size>0){
		s << " logs_size='" << get_size(conf.logs_size) << "'";
	}
	if (conf.log_handle) {
		s << " log_handle='1'";
	}
	if (conf.log_sub_request) {
		s << " log_sub_request='1'";
	}
	if (conf.log_radio > 0) {
		s << " radio='" << conf.log_radio << "'";
	}
	s << "/>\n";
	if (conf.http2https_code > 0) {
		s << "\t<http2https_code>" << conf.http2https_code << "</http2https_code>\n";
	}
	if (conf.auth_delay > 0) {
		s << "\t<auth_delay>" << conf.auth_delay << "</auth_delay>\n";
	}
	if(*conf.server_software){
		s << "\t<server_software>" << conf.server_software << "</server_software>\n";
	}
	if (*conf.hostname) {
		s << "\t<hostname>" << conf.hostname << "</hostname>\n";
	}
	if (conf.fiber_stack_size > 0) {
		s << "\t<fiber_stack_size>" << get_size(conf.fiber_stack_size) << "</fiber_stack_size>\n";
	}
	s << "\t<worker_io>" << conf.worker_io << "</worker_io>\n";
	s << "\t<max_io>" << conf.max_io << "</max_io>\n";
	s << "\t<io_timeout>" << conf.io_timeout << "</io_timeout>\n";
	s << "\t<worker_dns>" << conf.worker_dns << "</worker_dns>\n";	
	conf.gam->buildXML(s, CHAIN_SKIP_EXT);	
#ifdef ENABLE_WRITE_BACK
	writeBackManager.buildXML(s, CHAIN_SKIP_EXT);
#endif
	s << "\t<!--access start-->\n";
	kaccess[REQUEST].buildXML(s, (CHAIN_XML_DETAIL|CHAIN_SKIP_EXT));
	kaccess[RESPONSE].buildXML(s, (CHAIN_XML_DETAIL|CHAIN_SKIP_EXT));
	s << "\t<!--access end-->\n";
	conf.gvm->build(s);
	s << "</config>\n";
}
