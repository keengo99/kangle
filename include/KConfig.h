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
 *  Author: KangHongjiu <keengo99@gmail.com>  2011-07-18
 */
#ifndef KCONFIG_H
#define KCONFIG_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef _WIN32
#include <unistd.h>
#endif
#include <vector>
#include <list>
#include <string>
#include <map>
#include "ksocket.h"
#include "global.h"
#include "KMutex.h"
#include "KTimeMatch.h"
#include "extern.h"
#include "kasync_worker.h"
#include "kgl_ssl.h"
#include "khttp.h"
#include "KDynamicString.h"
#include "KAutoArray.h"
#include "KXmlAttribute.h"
#include "KConfigTree.h"
#include "serializable.h"

#define AUTOUPDATE_OFF    0
#define AUTOUPDATE_ON   1
#define AUTOUPDATE_DOWN   2

#define REFRESH_AUTO	0
#define	REFRESH_ANY		1	
#define REFRESH_NEVER	2

#define USE_PROXY	1
#define USE_LOG		(1<<1)
#define IGNORE_CASE	(1<<2)
#define NO_FILTER	(1<<2)
#define SAFE_STRCPY(s,s1)   do {memset(s,0,sizeof(s));strncpy(s,s1,sizeof(s)-1);}while(0)
class KAcserverManager;
class KVirtualHostManage;
class KVirtualHost;
struct KPerIpConnect;
class KDsoExtendManage;
class KHttpFilterManage;
class KSslCertificate
{
public:
	KSslCertificate() {

	}
#ifdef KSOCKET_SSL
	KSslCertificate(const KString& cert_file, const KString& key_file) : cert_file{ cert_file }, key_file{ key_file }{
	}
#endif
	void parse_certificate(const KXmlAttribute& attr) {
#ifdef KSOCKET_SSL
		cert_file = attr["certificate"];
		key_file = attr["certificate_key"];
#endif
	}
#ifdef KSOCKET_SSL
	void attach_modified_event(kconfig::KConfigFile* file, KXmlAttribute& attributes, const KString& doc_root);
	virtual ~KSslCertificate() {
	}
	KString cert_file;
	KString key_file;

	virtual KString get_cert_file() const;
	virtual KString get_key_file() const;
	KString get_cert_file(const KString& doc_root) const;
	KString get_key_file(const KString& doc_root) const;
#endif
};
class KSslConfig : public KSslCertificate
{
public:
#ifdef KSOCKET_SSL
	kgl_ssl_ctx* new_ssl_ctx(const KString &certfile, const KString &keyfile);
	virtual kgl_ssl_ctx* refs_ssl_ctx() {
		return new_ssl_ctx(get_cert_file(), get_key_file());
	}
	void dump(kgl::serializable* sl) {
		if (!cert_file.empty()) {
			sl->add("alpn", alpn);
			sl->add("early_data", early_data);
			sl->add("reject_nosni", reject_nosni);
			sl->add("cipher", cipher);
			sl->add("protocols", protocols);
			sl->add("certificate", cert_file);
			sl->add("certificate_key", key_file);
		}		
	}
	u_char alpn = 0;
	bool early_data = false;
	bool reject_nosni = false;
	KString cipher;
	KString protocols;
#endif
};

inline kgl_auto_cstr getPath(const char* file) {
	char* path = strdup(file);
	char* e = path + strlen(path);
	while (e > path) {
		if (*e == '/'
#ifdef _WIN32
			|| *e == '\\'
#endif
			) {
			*e = '\0';
			break;
		}
		e--;

	}
	return kgl_auto_cstr(path);

}
class KExtConfigDynamicString : public KDynamicString
{
public:
	KExtConfigDynamicString(const char* file) {
		this->file = file;
		path = getPath(file);
	}
	~KExtConfigDynamicString() {
	}
	const char* getValue(const char* name) {
		if (strcasecmp(name, "config_dir") == 0) {
			return path.get();
		}
		if (strcasecmp(name, "config_file") == 0) {
			return file;
		}
		return NULL;
	}
private:
	kgl_auto_cstr path;
	const char* file;
};
class KListenHost : public KSslConfig
{
public:
	KString ip;
	KString port;
	int model = 0;
};
using KSafeListen = std::unique_ptr<KListenHost>;
struct WorkerProcess
{
#ifdef _WIN32
#ifdef ENABLE_DETECT_WORKER_LOCK
	HANDLE active_event;
#endif
	HANDLE shutdown_event;
	HANDLE notice_event;
	HANDLE hProcess;
	int pid;
	int pending_count;
#else
	pid_t pid;
#endif
	bool closed;
	int worker_index;
};
class KConfigBase
{
public:
	KConfigBase();
	//unsigned int worker;
	unsigned int max;
	unsigned int max_per_ip;
	unsigned int per_ip_deny;
	unsigned min_free_thread;
	int http2https_code = 0;


	unsigned max_connect_info;
	//ֻѹ������cache�����
	int only_compress_cache;
	//��Сѹ������
	unsigned min_compress_length;
	//ѹ������(1-9)
	int gzip_level;
	int br_level;
	int zstd_level;
	bool path_info;
#ifdef ENABLE_ADPP
	int process_cpu_usage;
#endif
#ifdef ENABLE_TF_EXCHANGE
	INT64 max_post_size = 8388608;
#endif
	bool read_hup = true;
#ifdef KSOCKET_UNIX	
	bool unix_socket = true;
#endif
#ifdef ENABLE_VH_FLOW
	//�Զ�ˢ������ʱ��(��)
	int flush_flow_time;
#endif

	char error_url[64] = { 0 };
#ifdef ENABLE_BLACK_LIST
	//������ʱ��
	int wl_time = 1800;
	int bl_time;
	char block_ip_cmd[512];
	char unblock_ip_cmd[512];
	char flush_ip_cmd[512];
	char report_url[512];
#endif
	char disk_work_time[32];
	char upstream_sign[32];
	int upstream_sign_len;
};
class KConfig : public KConfigBase
{
public:
	~KConfig();
	KVirtualHostManage* vm;
	//run_user,run_groupΪһ����ʹ�ã����԰�ȫ��string
	KString run_user;
	KString run_group;
};
//���ò����ͳ���������Ϣ
class KGlobalConfig : public KConfig
{
public:
	KGlobalConfig();
	//////////////////////////////////////////////////////
	KMutex admin_lock;
	int auth_type;
	int passwd_crypt;
	KString admin_passwd;
	KString admin_user;
	std::vector<KString> admin_ips;
	/////////////////////////////////////////////////////////
	//���������õı�������ÿ�������ļ����ģ���Ҫ��������
	KTimeMatch diskWorkTime;
	/////////////////////////////////////////////////////////
	//�����ǳ���������Ϣ,ֻ�ڳ�������ǰ��ʼ��һ��...
	std::list<KString> mergeFiles;
	std::map<KString, std::vector<KSafeListen>> services;
	int worker_dns;
	int worker_io;
	//int fiber_stack_size;
	int max_io;
	unsigned io_buffer;
	//Ĭ���Ƿ񻺴�,1=��,����=��
	int default_cache = 1;
	unsigned max_cache_size = 1048576;
	INT64 mem_cache = 100 * 1024 * 1024;
#ifdef ENABLE_DISK_CACHE
	INT64 disk_cache = 0;
	bool disk_cache_is_radio = false;
#ifdef ENABLE_BIG_OBJECT_206
	bool cache_part = true;
#endif
	INT64 max_bigobj_size = 1024 * 1024 * 1024;
#endif
	char access_log[128] = { 0 };
	char log_rotate[32] = { 0 };
	char logHandle[512] = { 0 };
	char server_software[32] = { 0 };
	unsigned maxLogHandle = 0;
	int log_event_id = 0;
	int log_level = 2;
	INT64 log_rotate_size = 0;
	INT64 error_rotate_size = 0;
	unsigned logs_day = 0;
	//bool log_sub_request;
	bool log_handle = false;
	INT64 logs_size = 90;
#ifdef ENABLE_LOG_DRILL
	int log_drill = 0;
#endif
	int log_radio = 0;
	int refresh_time = 30;
	//ʵ���õ�ʱ����disk_cahce_dir.
	char disk_cache_dir2[512] = { 0 };
	char disk_cache_dir[512] = { 0 };
	char cookie_stick_name[16] = { 0 };
	char hostname[32] = { 0 };
	char lang[8] = { 0 };
	unsigned keep_alive_count = 0;
	unsigned time_out = 60;
	unsigned connect_time_out = 0;
	int auth_delay = 5;
	void set_connect_time_out(unsigned val) {
		connect_time_out = val;
		if (connect_time_out > 0 && connect_time_out < 2) {
			connect_time_out = 2;
		}
	}
	void set_time_out(unsigned val) {
		time_out = val;
		if (time_out < 5) {
			//��С��ʱΪ5��
			time_out = 5;
		}
	}
	KString path;
	KString tmppath;
	KString program;
	KString extworker;
	kasync_worker* ioWorker = nullptr;
	kasync_worker* dnsWorker = nullptr;
	int serverNameLength = 0;
	char serverName[32] = { 0 };
	int select_count = 0;
#ifdef MALLOCDEBUG
	bool mallocdebug = false;
#endif
#ifdef _WIN32
	//kangle�������ڵ��̷�
	KString diskName;
#endif
	KAcserverManager* gam;
	KVirtualHostManage* gvm;
	//3311��������������
	KVirtualHost* sysHost;
	KDsoExtendManage* dem;
};
extern KGlobalConfig conf;
extern int m_debug;
extern int kgl_cpu_number;
void do_config(bool first_time);
//��������ļ��������ڴ�й©���ʱ���ŵ���
void clean_config();
void wait_load_config_done();
bool saveConfig();
void parse_server_software();
INT64 get_size(const char* size);
KString get_size(INT64 size);
char* get_human_size(double size, char* buf, size_t buf_size);
INT64 get_radio_size(const char* size, bool& is_radio);

#endif
