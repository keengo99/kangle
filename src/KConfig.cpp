#include <map>
#include <assert.h>
#include <string>
#include <sstream>
#include "do_config.h"
#include "utils.h"
#include "log.h"
#include "kmd5.h"
#include "KHttpLib.h"
#include "lang.h"
#include "KConfigBuilder.h"
#include "KConfigParser.h"
#include "KModelManager.h"
#include "KAccess.h"
#include "KLang.h"
#include "KHttpServerParser.h"
#include "KContentType.h"
#include "kmalloc.h"
#include "KVirtualHostManage.h"
#include "KVirtualHostDatabase.h"
#include "KAcserverManager.h"
#include "KWriteBackManager.h"
#include "kselector_manager.h"
#include "KListenConfigParser.h"
#include "KDsoConfigParser.h"
#include "directory.h"
#include "kserver.h"
#include "KHtAccess.h"
#include "KLogHandle.h"
#include "cache.h"
#include "katom.h"
#include "kthread.h"
#include "kfiber.h"
#include "KHttpServer.h"
#include "KConfigTree.h"
#include "KHttpManage.h"
#include "KDefer.h"
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/mman.h>
#endif
volatile bool cur_config_ext = false;
static volatile int32_t load_config_progress = 0;
volatile bool configReload = false;
using namespace std;
KConfig* cconf = NULL;
KGlobalConfig conf;
//读取配置文件，可重入
void load_config(KConfig* cconf, bool firstTime);
void post_load_config(bool firstTime);
#ifdef KSOCKET_SSL
kgl_ssl_ctx* KSslConfig::new_ssl_ctx(const char* certfile, const char* keyfile) {
	void* ssl_ctx_data = NULL;
	kgl_ssl_ctx* ssl_ctx = kgl_new_ssl_ctx(NULL);
	ssl_ctx->alpn = alpn;
#ifdef ENABLE_HTTP2
	ssl_ctx_data = &ssl_ctx->alpn;
#endif
	SSL_CTX* ctx = kgl_ssl_ctx_new_server(certfile,
		keyfile,
		NULL,
		NULL,
		ssl_ctx_data);
	if (ctx == NULL) {
		klog(KLOG_ERR,
			"Cann't init ssl context certificate=[%s],certificate_key=[%s]\n",
			certfile ? certfile : "",
			keyfile ? keyfile : "");
		kgl_release_ssl_ctx(ssl_ctx);
		return NULL;
	}
	if (early_data) {
		kgl_ssl_ctx_set_early_data(ctx, true);
	}
	if (!cipher.empty()) {
		if (!kgl_ssl_ctx_set_cipher_list(ctx, cipher.c_str())) {
			klog(KLOG_WARNING, "cipher [%s] is not support\n", cipher.c_str());
		}
	}
	if (!protocols.empty()) {
		kgl_ssl_ctx_set_protocols(ctx, protocols.c_str());
	}
	ssl_ctx->ctx = ctx;
	return ssl_ctx;
}
std::string KSslConfig::get_cert_file() {
	if (isAbsolutePath(cert_file.c_str())) {
		return cert_file;
	}
	return  conf.path + cert_file;
}
std::string KSslConfig::get_key_file() {
	if (isAbsolutePath(key_file.c_str())) {
		return key_file;
	}
	return  conf.path + key_file;
}
#endif
KConfigBase::KConfigBase() {
	memset(this, 0, sizeof(KConfigBase));
}
void KConfig::copy(KConfig* c) {
	//把cconf赋值到conf中
	KConfigBase* bc = static_cast<KConfigBase*>(this);
	kgl_memcpy(bc, static_cast<KConfigBase*>(c), sizeof(KConfigBase));
	this->run_user = c->run_user;
	this->run_group = c->run_group;
	this->apache_config_file = c->apache_config_file;
	return;
}
KConfig::~KConfig() {

}
KGlobalConfig::KGlobalConfig() {
	gam = new KAcserverManager;
	gvm = new KVirtualHostManage;
	sysHost = new KVirtualHost;
	dem = NULL;
	select_count = 0;
}

class KExtConfig
{
public:
	KExtConfig(char* content) {
		this->content = content;
		next = NULL;
	}
	~KExtConfig() {
		if (content) {
			xfree(content);
		}
		if (next) {
			delete next;
		}
	}
	std::string file;
	char* content;
	bool merge;
	KExtConfig* next;
};
static map<int, KExtConfig*> extconfigs;
bool get_size_radio(INT64 size, int radio, const char radio_char, std::stringstream& s) {
	INT64 t;
	t = size >> radio;
	if (t > 0) {
		if ((t << radio) == size) {
			s << t << radio_char;
			return true;
		}
	}
	return false;
}
char* get_human_size(double size, char* buf, size_t buf_size) {
	memset(buf, 0, buf_size);
	int i = 0;
	const char* units[] = { "", "K", "M", "G", "T", "P", "E", "Z", "Y" };
	while (size > 1024) {
		if (i > 8) {
			break;
		}
		size /= 1024;
		i++;
	}
	snprintf(buf, buf_size - 1, "%.*f%s", 2, size, units[i]);
	return buf;
}
std::string get_size(INT64 size) {
	std::stringstream s;
	if (get_size_radio(size, 40, 'T', s)) {
		return s.str();
	}
	if (get_size_radio(size, 30, 'G', s)) {
		return s.str();
	}
	if (get_size_radio(size, 20, 'M', s)) {
		return s.str();
	}
	if (get_size_radio(size, 10, 'K', s)) {
		return s.str();
	}
	s << size;
	return s.str();
}
INT64 get_radio_size(const char* size, bool& is_radio) {
	is_radio = false;
	INT64 cache_size = string2int(size);
	int len = (int)strlen(size);
	char t = 0;
	for (int i = 0; i < len; i++) {
		if (size[i] == '.') {
			continue;
		}
		if (!(size[i] >= '0' && size[i] <= '9')) {
			t = size[i];
			break;
		}
	}
	switch (t) {
	case 'k':
	case 'K':
		return cache_size << 10;
	case 'm':
	case 'M':
		return cache_size << 20;
	case 'g':
	case 'G':
		return cache_size << 30;
	case 't':
	case 'T':
		return cache_size << 40;
	case '%':
		is_radio = true;
		if (cache_size <= 0) {
			cache_size = 0;
		}
		if (cache_size >= 100) {
			cache_size = 100;
		}
		break;
	}
	return cache_size;
}
INT64 get_size(const char* size) {
	bool is_radio = false;
	return get_radio_size(size, is_radio);
}
void init_config(KConfig* conf) {
	conf->max = 50000;
	conf->path_info = true;
	conf->passwd_crypt = CRYPT_TYPE_PLAIN;
	conf->gzip_level = 5;
	conf->br_level = 5;
	conf->min_compress_length = 512;
	conf->fiber_stack_size = 0;
	conf->wl_time = 1800;
#ifdef ENABLE_TF_EXCHANGE
	conf->max_post_size = 8388608;
#endif	
	conf->read_hup = true;
	conf->max_io = 0;
	conf->worker_io = 16;
	conf->worker_dns = 32;
	conf->io_buffer = 262144;
}
void LoadDefaultConfig() {
	init_config(&conf);
	conf.ioWorker = NULL;
	conf.dnsWorker = NULL;
#ifdef ENABLE_DISK_CACHE
	conf.diskWorkTime.set(NULL);
#endif
#ifdef KANGLE_WEBADMIN_DIR
	conf.sysHost->doc_root = KANGLE_WEBADMIN_DIR;
#else
	conf.sysHost->doc_root = conf.path;
	conf.sysHost->doc_root += "webadmin";
#endif
	conf.sysHost->browse = false;
	KSubVirtualHost* svh = new KSubVirtualHost(conf.sysHost);
	svh->setDocRoot(conf.sysHost->doc_root.c_str(), "/");
#ifdef HTTP_PROXY
	//add mime type
	conf.sysHost->addMimeType("gif", "image/gif", kgl_compress_never, 0);
	conf.sysHost->addMimeType("css", "text/css", kgl_compress_on, 0);
	conf.sysHost->addMimeType("html", "text/html", kgl_compress_on, 0);
	conf.sysHost->addMimeType("js", "text/javascript", kgl_compress_on, 0);
	conf.sysHost->addMimeType("*", "text/plain", kgl_compress_unknow, 0);
#endif
	conf.sysHost->hosts.push_back(svh);
	conf.sysHost->addRef();
	init_manager_handler();
}
void loadExtConfigFile(int index, KExtConfig* config, KXml& xmlParser) {
	while (config) {
		if (config->merge) {
			cur_config_ext = false;
		} else {
			cur_config_ext = true;
		}
		klog(KLOG_NOTICE, "[%d] load config file [%s]\n", index, config->file.c_str());
		try {
			xmlParser.startParse(config->content);
		} catch (KXmlException& e) {
			fprintf(stderr, "%s in file[%s]\n", e.what(), config->file.c_str());
		}
		config = config->next;
	}
}
bool load_config_file(KFileName* file, int inclevel, KStringBuf& s, int& id, bool& merge) {
	if (inclevel > 128) {
		klog(KLOG_ERR, "include level [%d] is limited.\n", inclevel);
		return false;
	}
	int len = (int)file->get_file_size();
	if (len <= 0 || len > 1048576) {
		klog(KLOG_ERR, "config file [%s] length is wrong\n", file->getName());
		return false;
	}
	KFile fp;
	if (!fp.open(file->getName(), fileRead)) {
		klog(KLOG_ERR, "cann't open file[%s]\n", file->getName());
		return false;
	}
	char* buf = (char*)malloc(len + 1);
	int read_len = fp.read(buf, len);
	if (read_len != len) {
		klog(KLOG_ERR, "this sure not be happen,read file [%s] size error.\n", file->getName());
		xfree(buf);
		return false;
	}
	buf[len] = '\0';
	KExtConfigDynamicString ds(file->getName());
	ds.dimModel = false;
	ds.blockModel = false;
	ds.envChar = '%';
	char* content = ds.parseDirect(buf);
	free(buf);
	char* hot = content;
	while (*hot && isspace((unsigned char)*hot)) {
		hot++;
	}
	char* start = hot;
	//默认启动顺序为50
	id = 50;
	if (strncmp(hot, "<!--#", 5) == 0) {
		hot += 5;
		if (strncmp(hot, "stop", 4) == 0) {
			/*
			 * 扩展没有启动
			 */
			xfree(content);
			return false;
		} else if (strncmp(hot, "start", 5) == 0) {
			char* end = strchr(hot, '>');
			if (end) {
				start = end + 1;
			}
			hot += 6;
			id = atoi(hot);
			char* p = strchr(hot, ' ');
			if (p) {
				if (inclevel > 0 && strncmp(p + 1, "merge", 5) == 0) {
					merge = true;
					conf.mergeFiles.push_back(file->getName());
				}
			}
		}
	}
	klog(KLOG_NOTICE, "read config file [%s] success\n", file->getName());
	hot = start;
	for (;;) {
		char* p = strstr(hot, "<!--#include");
		if (p == NULL) {
			s << hot;
			break;
		}
		int pre_hot_len = (int)(p - hot);
		p += 12;
		s.write_all(hot, pre_hot_len);
		hot = strstr(p, "-->");
		if (hot == NULL) {
			break;
		}
		while (p < hot&& isspace((unsigned char)*p)) {
			p++;
		}
		int filelen = (int)(hot - p);
		if (filelen <= 0) {
			break;
		}
		char* incfilename = (char*)xmalloc(filelen + 1);
		kgl_memcpy(incfilename, p, filelen);
		incfilename[filelen] = '\0';
		for (int i = filelen - 1; i > 0; i--) {
			if (!isspace((unsigned char)incfilename[i])) {
				break;
			}
			incfilename[i] = '\0';
		}
		bool incresult = false;
		char* translate_filename = ds.parseDirect(incfilename);
		xfree(incfilename);
		KFileName incfile;
		if (translate_filename) {
			if (!isAbsolutePath(translate_filename)) {
				incresult = incfile.setName(conf.path.c_str(), translate_filename, FOLLOW_LINK_ALL);
			} else {
				incresult = incfile.setName(translate_filename);
			}
			xfree(translate_filename);
		}
		if (incresult) {
			int id;
			bool merge = false;
			load_config_file(&incfile, inclevel + 1, s, id, merge);
		}
		hot += 3;
	}
	xfree(content);
	return true;
}
void loadExtConfigFile(KFileName* file) {
	KStringBuf s;
	int id = 50;
	bool merge = false;
	if (!load_config_file(file, 0, s, id, merge)) {
		return;
	}
	KExtConfig* extconf = new KExtConfig(s.steal());
	extconf->file = file->getName();
	extconf->merge = merge;
	map<int, KExtConfig*>::iterator it;
	it = extconfigs.find(id);
	if (it != extconfigs.end()) {
		extconf->next = (*it).second->next;
		(*it).second->next = extconf;
	} else {
		extconfigs.insert(pair<int, KExtConfig*>(id, extconf));
	}
}
int handleExtConfigFile(const char* file, void* param) {
	KFileName configFile;
	if (!configFile.setName((char*)param, file, FOLLOW_LINK_ALL)) {
		return 0;
	}
	if (configFile.isDirectory()) {
		KFileName dirConfigFile;
		if (!dirConfigFile.setName(configFile.getName(), "config.xml",
			FOLLOW_LINK_ALL)) {
			return 0;
		}
		loadExtConfigFile(&dirConfigFile);
		return 0;
	}
	loadExtConfigFile(&configFile);
	return 0;
}
void loadExtConfigFile() {
#ifdef KANGLE_EXT_DIR
	std::string ext_path = KANGLE_EXT_DIR;
#else
	std::string ext_path = conf.path + "/ext";
#endif
	list_dir(ext_path.c_str(), handleExtConfigFile, (void*)ext_path.c_str());
	string configFile = conf.path + "etc/vh.d/";
	list_dir(configFile.c_str(), handleExtConfigFile, (void*)configFile.c_str());
}
bool saveConfig() {
	return KConfigBuilder::saveConfig();
}
void load_main_config(KConfig* cconf, std::string& configFile, KXml& xmlParser, bool firstload) {
	klog(KLOG_NOTICE, "load config file [%s]\n", configFile.c_str());
	for (int i = 0; i < 2; i++) {
		try {
			xmlParser.parseFile(configFile);
			break;
		} catch (KXmlException& e) {
			fprintf(stderr, "%s\n", e.what());
			if (i > 0) {
				exit(0);
			} else {
				printf("cann't read config.xml try to read config.xml.lst\n");
				configFile = configFile + ".lst";
			}
		}
	}
	try {
#ifdef KANGLE_WEBADMIN_DIR
		configFile = KANGLE_WEBADMIN_DIR;
#else
		configFile = conf.path + "/webadmin";
#endif
		configFile += "/lang.xml";
		klog(KLOG_NOTICE, "load config file [%s]\n", configFile.c_str());
		klang.load(configFile.c_str());
	} catch (KXmlException& e) {
		fprintf(stderr, "%s\n", e.what());
	}
}
void clean_config() {
	printf("clean_config now.........\n");
	kaccess[REQUEST].destroy();
	kaccess[RESPONSE].destroy();
	conf.admin_ips.clear();
	conf.admin_passwd.clear();
	conf.admin_user.clear();
	for (auto it = conf.services.begin(); it != conf.services.end(); ++it) {
		for (uint32_t index = 0;; ++index) {
			auto lh = (*it).second.get(index);
			if (!lh) {
				break;
			}
			conf.gvm->UnBindGlobalListen(lh);
		}
	}
	conf.services.clear();
#ifdef ENABLE_WRITE_BACK
	writeBackManager.destroy();
#endif
}
bool on_begin_parse(kconfig::KConfigFile* file, khttpd::KXmlNode* node) {
	if (!kconfig::is_first()) {
		return true;
	}
	if (file->is_default()) {
#ifdef MALLOCDEBUG
		auto malloc_debug = kconfig::find_child(node, _KS("mallocdebug"));
		if (malloc_debug) {
			conf.mallocdebug = atoi(malloc_debug->get_text()) == 1;
		}
		if (conf.mallocdebug) {
			start_hook_alloc();
		}
#endif
		auto worker_thread = kconfig::find_child(node, _KS("worker_thread"));
		if (worker_thread) {
			conf.select_count = atoi(worker_thread->get_text());
		}
		int select_count = conf.select_count;
		if (select_count <= 0) {
			select_count = kgl_cpu_number;
		}
		if (select_count > 1) {
			if (!selector_manager_grow(select_count)) {
				fprintf(stderr, "cann't grow worker thread..\n");
			} else {
				fprintf(stderr, "worker thread grow to [%d] success.\n", get_selector_count());
			}
		}

	}
	return true;
}
int do_config_thread(void* first_time, int argc) {
	assert(kfiber_self());
	cur_config_ext = false;
	if (first_time != NULL) {
#ifdef _WIN32
		_setmaxstdio(2048);
#endif
		for (int i = 0; i < 2; i++) {
			kaccess[i].setType(i);
			kaccess[i].setGlobal(true);
		}
		KAccess::loadModel();
		kconfig::init(on_begin_parse);
		kconfig::listen(_KS("dso_extend"), nullptr, on_dso_event, kconfig::ev_subdir);
		kconfig::listen(_KS(""), &conf, on_main_event, kconfig::ev_subdir);
		kconfig::listen(_KS("server"), conf.gam, on_server_event, kconfig::ev_subdir);
		kconfig::listen(_KS("listen"), nullptr, on_listen_event, kconfig::ev_self | kconfig::ev_merge);
		kconfig::listen(_KS("ssl_client"), nullptr, on_ssl_client_event, kconfig::ev_self);
		kconfig::listen(_KS("admin"), nullptr, on_admin_event, kconfig::ev_self);
		kconfig::listen(_KS("log"), nullptr, on_log_event, kconfig::ev_self | kconfig::ev_at_once);
		kconfig::listen(_KS("cache"), nullptr, on_cache_event, kconfig::ev_self | kconfig::ev_at_once);
	}
	assert(cconf == NULL);
	cconf = new KConfig;
	defer(delete cconf; cconf = NULL);
	init_config(cconf);
	//new version config
	kconfig::reload();
	load_config(cconf, first_time != NULL);
	post_load_config(first_time != NULL);
	katom_set((void*)&load_config_progress, 0);
	return 0;
}
void do_config(bool first_time) {
	if (!katom_cas((void*)&load_config_progress, 0, 1)) {
		assert(!first_time);
		return;
	}
	if (kfiber_create(do_config_thread, first_time ? (void*)&first_time : nullptr, 0, conf.fiber_stack_size, NULL) != 0) {
		katom_set((void*)&load_config_progress, 0);
	}
}
void wait_load_config_done() {
	for (;;) {
		if (0 == katom_get((void*)&load_config_progress)) {
			break;
		}
		kfiber_msleep(10);
	}
}
int merge_apache_config(const char* filename) {
	KFileName file;
	if (file.setName(filename)) {
		KApacheConfig apConfig(false);
		std::stringstream s;
		s << "<!--#start 1000 merge-->\n";
		apConfig.load(&file, s);
		std::stringstream xf;
		xf << conf.path << PATH_SPLIT_CHAR << "ext";
		mkdir(xf.str().c_str(), 0755);
		xf << "/_apache.xml";
		FILE* fp = fopen(xf.str().c_str(), "wt");
		if (fp == NULL) {
			fprintf(stderr, "cann't open file [%s] to write\n", xf.str().c_str());
			return 1;
		}
		fwrite(s.str().c_str(), 1, s.str().size(), fp);
		fclose(fp);
		fprintf(stdout, "success convert to file [%s] please reboot kangle\n", xf.str().c_str());
		return 0;
	} else {
		fprintf(stderr, "cann't open apache config file [%s]\n", filename);
		return 1;
	}
}
void load_db_vhost(KVirtualHostManage* vm) {
#ifndef HTTP_PROXY
	cur_config_vh_db = false;
	if (vhd.isLoad()) {
		string errMsg;
		if (!vhd.loadVirtualHost(vm, errMsg)) {
			klog(KLOG_ERR, "Cann't load VirtualHost[%s]\n", errMsg.c_str());
		}
	}
#endif
}
//读取配置文件，可重入
void load_config(KConfig* cconf, bool firstTime) {
	std::map<int, KExtConfig*>::iterator it;
	bool main_config_loaded = false;
#ifdef KANGLE_ETC_DIR
	std::string configFileDir = KANGLE_ETC_DIR;
#else
	std::string configFileDir = conf.path + "/etc";
#endif
	std::string configFile = configFileDir + CONFIG_FILE;
	loadExtConfigFile();
	KConfigParser parser;
	KAccess access[2];
	KAcserverManager am;
	KWriteBackManager wm;
	KVirtualHostManage vm;
	KXml xmlParser;
	xmlParser.setData(cconf);
	xmlParser.addEvent(&parser);
	//xmlParser.addEvent(&listenConfigParser);
#ifndef HTTP_PROXY
	KHttpServerParser vhParser;
	xmlParser.addEvent(&vhParser);
#endif
	if (firstTime) {
#ifdef ENABLE_BLACK_LIST
		init_run_fw_cmd();
#endif
		xmlParser.addEvent(conf.gam);
#ifdef ENABLE_WRITE_BACK
		xmlParser.addEvent(&writeBackManager);
#endif
#ifdef HTTP_PROXY
		xmlParser.addEvent(&kaccess[0]);
		xmlParser.addEvent(&kaccess[1]);
#endif
		cconf->am = conf.gam;
		cconf->vm = &vm;
		//dso配置只在启动时加载一次
	} else {
		for (int i = 0; i < 2; i++) {
			access[i].setType(i);
			access[i].setGlobal(true);
		}
#ifndef HTTP_PROXY
		vhParser.kaccess[0] = &access[0];
		vhParser.kaccess[1] = &access[1];
#else
		xmlParser.addEvent(&access[0]);
		xmlParser.addEvent(&access[1]);
#endif
		xmlParser.addEvent(&am);
		xmlParser.addEvent(&wm);
		cconf->am = &am;
		cconf->vm = &vm;
	}

	for (it = extconfigs.begin(); it != extconfigs.end(); it++) {
		if (!main_config_loaded && (*it).first >= 100) {
			main_config_loaded = true;
			cur_config_ext = false;
			load_main_config(cconf, configFile, xmlParser, firstTime);
			cur_config_ext = true;
		}
		loadExtConfigFile((*it).first, (*it).second, xmlParser);
		delete (*it).second;
	}
	extconfigs.clear();
	cur_config_ext = false;
	if (!main_config_loaded) {
		load_main_config(cconf, configFile, xmlParser, firstTime);
	}
#ifndef KANGLE_FREE
	if (conf.apache_config_file.size() > 0) {
		KFileName file;
		if (file.setName(conf.apache_config_file.c_str())) {
			KApacheConfig apConfig(false);
			std::stringstream s;
			apConfig.load(&file, s);
			cur_config_vh_db = true;
			char* content = strdup(s.str().c_str());
			KExtConfig apextconfig(content);
			apextconfig.merge = false;
			apextconfig.file = conf.apache_config_file;
			loadExtConfigFile(999999, &apextconfig, xmlParser);
		}
	}
#endif
	//conf.gvm->BindGlobalListens(cconf->service);
	//conf.gvm->UnBindGlobalListens(conf.service);
	load_db_vhost(cconf->vm);
	conf.copy(cconf);
	conf.gvm->copy(&vm);
	if (!firstTime) {
		conf.gam->copy(am);
		writeBackManager.copy(wm);
		for (int i = 0; i < 2; i++) {
			//access[i].setChainAction();
			kaccess[i].copy(access[i]);
		}
	}
	// load serial
#if 0
	string serial_file = conf.path;
	serial_file += ".autoupdate.conf";
	FILE* fp = fopen(serial_file.c_str(), "rt");
	if (fp) {
		fscanf(fp, "%d", &serial);
		fclose(fp);
	}
#endif
	cur_config_ext = false;
}
void parse_server_software() {
	//生成serverName
	timeLock.Lock();
	if (*conf.server_software) {
		SAFE_STRCPY(conf.serverName, conf.server_software);
	} else {
		std::string serverName = PROGRAM_NAME;
		serverName += "/";
		serverName += VERSION;
		SAFE_STRCPY(conf.serverName, serverName.c_str());
	}
	conf.serverNameLength = (int)strlen(conf.serverName);
	timeLock.Unlock();
}

void post_load_config(bool firstTime) {
	if (conf.worker_io < 2) {
		conf.worker_io = 2;
	}
	if (conf.worker_dns < 2) {
		conf.worker_dns = 2;
	}
	if (conf.ioWorker == NULL) {
		conf.ioWorker = kasync_worker_init(conf.worker_io, conf.max_io);
	} else {
		kasync_worker_set(conf.ioWorker, conf.worker_io, conf.max_io);
	}
	if (conf.dnsWorker == NULL) {
		conf.dnsWorker = kasync_worker_init(conf.worker_dns, 512);
	} else {
		kasync_worker_set(conf.dnsWorker, conf.worker_dns, 512);
	}
	parse_server_software();
	http_config.fiber_stack_size = conf.fiber_stack_size;
	cache.init(firstTime);
#ifdef ENABLE_BLACK_LIST
	if (firstTime && *conf.flush_ip_cmd) {
		run_fw_cmd(conf.flush_ip_cmd, NULL);
	}
	conf.gvm->globalVh.blackList->setRunFwCmd(*conf.block_ip_cmd != '\0');
	conf.gvm->globalVh.blackList->setReportIp(*conf.report_url != '\0');
#endif
}
