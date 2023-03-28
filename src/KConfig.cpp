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
#include "KDsoExtendManage.h"
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/mman.h>
#endif
static volatile int32_t load_config_progress = 0;
volatile bool configReload = false;
using namespace std;
KGlobalConfig conf;
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
KString KSslCertificate::get_cert_file(const KString& doc_root) const {
	if (cert_file.empty()) {
		return cert_file;
	}
	if (cert_file[0] == '-') {
		return conf.path + (cert_file.c_str() + 1);
	}
	if (!isAbsolutePath(cert_file.c_str())) {
		return doc_root + cert_file;
	}
	return cert_file;
}
KString KSslCertificate::get_key_file(const KString& doc_root) const {
	if (key_file.empty()) {
		return key_file;
	}
	if (key_file[0] == '-') {
		return conf.path + (key_file.c_str() + 1);
	}
	if (!isAbsolutePath(key_file.c_str())) {
		return doc_root + key_file;
	}
	return key_file;
}
KString KSslCertificate::get_cert_file() const {
	return get_cert_file(conf.path);
}
KString KSslCertificate::get_key_file() const {
	return get_key_file(conf.path);
}
#endif
KConfigBase::KConfigBase() {
	memset(this, 0, sizeof(KConfigBase));
}

KConfig::~KConfig() {

}
KGlobalConfig::KGlobalConfig() {
	gam = new KAcserverManager;
	gvm = new KVirtualHostManage;
	sysHost = new KVirtualHost("_SYS");
	dem = NULL;
	select_count = 0;
}

bool get_size_radio(INT64 size, int radio, const char radio_char, KWStream& s) {
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
KString get_size(INT64 size) {
	KStringBuf s;
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
void upgrade_vh_map(khttpd::KXmlNode* node) {
	auto node_body = node->get_first();
	auto it = kconfig::find_first_child(node_body, "map"_CS);
	if (!it) {
		return;
	}
	auto map_node = it->value();
	for (auto&& body : map_node->body) {
		khttpd::KXmlNodeBody* new_node_body;
		auto file_ext = body->attributes.remove("file_ext");
		if (file_ext) {
			new_node_body = kconfig::new_child(node_body, _KS("map_file"));
			body->attributes.emplace("ext", file_ext);
		} else {
			new_node_body = kconfig::new_child(node_body, _KS("map_path"));
		}
		new_node_body->attributes.swap(body->attributes);
		new_node_body->childs.swap(body->childs);
	}
	map_node->release();
	node_body->childs.erase(it);
}
void upgrade_chain_access(const kgl_ref_str_t& access, khttpd::KXmlNode* node) {
	node = kconfig::find_child(node->get_first(), access.data, access.len);
	if (!node) {
		return;
	}
	for (auto it = kconfig::find_first_child(node->get_first(), "table"_CS); it && it->value()->is_tag("table"_CS); it = it->next()) {
		auto chain = kconfig::find_child(it->value()->get_first(), _KS("chain"));
		if (!chain) {
			continue;
		}
		for (auto&& module : chain->body) {
			for (auto it2 = module->childs.first(); it2;) {
				auto it_next = it2->next();
				auto module_node = it2->value();
				if (kgl_ncasecmp(module_node->key.tag->data, module_node->key.tag->len, _KS("acl_")) == 0) {
					for (auto&& body : module_node->body) {
						auto new_module = kconfig::new_child(module, _KS("acl"));
						body->clone_to(new_module);
						new_module->attributes.emplace(KString(_KS("module")), KString(module_node->key.tag->data + 4, module_node->key.tag->len - 4));
					}
					module_node->release();
					module->childs.erase(it2);
				} else if (kgl_ncasecmp(module_node->key.tag->data, module_node->key.tag->len, _KS("mark_")) == 0) {
					for (auto&& body : module_node->body) {
						auto new_module = kconfig::new_child(module, _KS("mark"));
						body->clone_to(new_module);
						new_module->attributes.emplace(KString(_KS("module")), KString(module_node->key.tag->data + 5, module_node->key.tag->len - 5));
					}
					module_node->release();
					module->childs.erase(it2);
				}
				it2 = it_next;
			}
		}
	}
}
static bool on_begin_parse(kconfig::KConfigFile* file, khttpd::KXmlNode* node) {
	if (kconfig::is_first()) {
		if (file->is_default()) {
#ifdef MALLOCDEBUG
			auto malloc_debug = kconfig::find_child(node->get_first(), _KS("mallocdebug"));
			if (malloc_debug) {
				conf.mallocdebug = atoi(malloc_debug->get_text()) == 1;
			}
			if (conf.mallocdebug) {
				start_hook_alloc();
			}
#endif
			auto worker_thread = kconfig::find_child(node->get_first(), _KS("worker_thread"));
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
		auto vh_database = kconfig::find_child(node->get_first(), _KS("vh_database"));
		if (vh_database) {
			vhd.parse_config(vh_database->get_first());
		}
	}
#ifdef KSOCKET_SSL
	//bind listen
	auto it = kconfig::find_first_child(node->get_first(), "listen"_CS);
	if (it) {
		//TODO handle
		auto xml = it->value();
		for (auto&& body : xml->body) {
			KSslCertificate ssl_cert;
			ssl_cert.parse_certificate(body->attributes);
			if (ssl_cert.cert_file) {
				KStringBuf name,value;
				name << khttpd::internal_xml_attribute << "cert_file";
				value << kfile_last_modified(ssl_cert.get_cert_file().c_str());
				body->attributes.emplace(name.str(), value.str());
			}
			if (ssl_cert.key_file) {
				KStringBuf name, value;
				name << khttpd::internal_xml_attribute << "key_file";
				value << kfile_last_modified(ssl_cert.get_key_file().c_str());
				body->attributes.emplace(name.str(), value.str());
			}
		}
	}
#endif	
	for(auto it = kconfig::find_first_child(node->get_first(), "vh"_CS);it && it->value()->is_tag(_KS("vh")); it = it->next()) {
		auto vh_node = it->value();
		upgrade_chain_access("request"_CS, vh_node);
		upgrade_chain_access("response"_CS, vh_node);
		upgrade_vh_map(vh_node);
		//printf("vh name=[%s]\n", it->value()->attributes()("name"));
		//TODO;
	}
	upgrade_chain_access("request"_CS, node);
	upgrade_chain_access("response"_CS, node);
	auto vhs = kconfig::find_child(node->get_first(), _KS("vhs"));
	if (vhs) {
		upgrade_vh_map(vhs);
	}
	return true;
}
static void init_config() {
	conf.path_info = true;
	conf.wl_time = 1800;
#ifdef ENABLE_TF_EXCHANGE
	conf.max_post_size = 8388608;
#endif
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
	conf.sysHost->add_ref();
	init_manager_handler();
#ifdef _WIN32
	_setmaxstdio(2048);
#endif
	kconfig::register_source_driver(kconfig::KConfigFileSource::Htaccess, new KApacheConfigDriver());
	kconfig::register_source_driver(kconfig::KConfigFileSource::Db, &vhd);
	kconfig::init(on_begin_parse);
	parse_server_software();
#ifdef ENABLE_BLACK_LIST
	init_run_fw_cmd();
#endif

	static auto main_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) -> bool {
		on_main_event(tree, ev);
		return true;
		}, kconfig::ev_subdir);
	kconfig::listen(_KS(""), &main_config_listener);

	conf.dem = new KDsoExtendManage;
	kconfig::listen(_KS("dso_extend"), conf.dem);
	static auto server_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) -> bool {
		conf.gam->on_server_event(tree, ev->get_xml(), ev->type);
		return true;
		}, kconfig::ev_subdir | kconfig::ev_skip_vary);
	kconfig::listen(_KS("server"), &server_config_listener);
#ifdef ENABLE_VH_RUN_AS
	static auto cmd_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) -> bool {
		conf.gam->on_cmd_event(tree, ev->get_xml(), ev->type);
		return true;
		}, kconfig::ev_subdir | kconfig::ev_skip_vary);
	kconfig::listen(_KS("cmd"), &cmd_config_listener);
#endif
	static auto api_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) -> bool {
		conf.gam->on_api_event(tree, ev->get_xml(), ev->type);
		return true;
		}, kconfig::ev_subdir | kconfig::ev_skip_vary);
	kconfig::listen(_KS("api"), &api_config_listener);

	static auto listen_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) -> bool {
		on_listen_event(tree, ev);
		return true;
		}, kconfig::ev_self | kconfig::ev_merge);
	kconfig::listen(_KS("listen"), &listen_config_listener);

	static auto ssl_client_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) -> bool {
		on_ssl_client_event(tree, ev);
		return true;
		}, kconfig::ev_self);
	kconfig::listen(_KS("ssl_client"), &ssl_client_config_listener);
	static auto admin_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev)->bool {
		on_admin_event(tree, ev);
		return true;
		}, kconfig::ev_self);
	kconfig::listen(_KS("admin"), &admin_config_listener);
	static auto log_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev)->bool {
		on_log_event(tree, ev);
		return true;
		}, kconfig::ev_self | kconfig::ev_at_once);
	kconfig::listen(_KS("log"), &log_config_listener);

	static auto cache_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev)->bool {
		on_cache_event(tree, ev);
		return true;
		}, kconfig::ev_self | kconfig::ev_at_once);
	kconfig::listen(_KS("cache"), &cache_config_listener);

	static auto io_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev)->bool {
		on_io_event(tree, ev);
		return true;
		}, kconfig::ev_self | kconfig::ev_at_once);
	kconfig::listen(_KS("io"), &io_config_listener);

	static auto fiber_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev)->bool {
		on_fiber_event(tree, ev);
		return true;
		}, kconfig::ev_self | kconfig::ev_at_once);
	kconfig::listen(_KS("fiber"), &fiber_config_listener);

	static auto dns_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev)->bool {
		on_dns_event(tree, ev);
		return true;
		}, kconfig::ev_self | kconfig::ev_at_once);
	kconfig::listen(_KS("dns"), &dns_config_listener);

	static auto connect_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev)->bool {
		on_connect_event(tree, ev);
		return true;
		}, kconfig::ev_self | kconfig::ev_at_once);
	kconfig::listen(_KS("connect"), &connect_config_listener);

	static auto run_as_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev)->bool {
		on_run_event(tree, ev);
		return true;
		}, kconfig::ev_self | kconfig::ev_at_once);
	kconfig::listen(_KS("run_as"), &run_as_config_listener);

	static auto compress_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev)->bool {
		on_compress_event(tree, ev);
		return true;
		}, kconfig::ev_self | kconfig::ev_at_once);
	kconfig::listen(_KS("compress"), &compress_config_listener);


	kconfig::listen(_KS("vhs"), &conf.gvm->vhs);
	kconfig::listen(_KS("vh"), conf.gvm);
#ifdef ENABLE_SVH_SSL
	kconfig::listen(_KS("ssl"), conf.gvm);
#endif
#ifdef ENABLE_BLACK_LIST
	static auto firewall_config_listener = kconfig::config_listen([](kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev)->bool {
		on_firewall_event(tree, ev);
		return true;
		}, kconfig::ev_self | kconfig::ev_at_once);
	kconfig::listen(_KS("firewall"), &firewall_config_listener);
#endif
	auto ev_root = kconfig::find(_KS(""));
	if (ev_root->add(_KS("request"), kaccess[REQUEST]) != nullptr) {
		kaccess[REQUEST]->add_ref();
	}
	if (ev_root->add(_KS("response"), kaccess[RESPONSE]) != nullptr) {
		kaccess[RESPONSE]->add_ref();
	}
}
bool saveConfig() {
	return false;
}
void clean_config() {
	printf("clean_config now.........\n");
	kaccess[REQUEST]->clear();
	kaccess[RESPONSE]->clear();
	kaccess[REQUEST]->release();
	kaccess[RESPONSE]->release();
	kaccess[0] = nullptr;
	kaccess[1] = nullptr;

	conf.admin_ips.clear();
	conf.admin_passwd.clear();
	conf.admin_user.clear();
	for (auto it = conf.services.begin(); it != conf.services.end(); ++it) {
		for (auto&& lh : (*it).second) {
			conf.gvm->UnBindGlobalListen(lh.get());
		}
	}
	conf.services.clear();
#ifdef ENABLE_WRITE_BACK
	writeBackManager.destroy();
#endif
}
void load_lang() {
	try {
#ifdef KANGLE_WEBADMIN_DIR
		KString configFile = KANGLE_WEBADMIN_DIR;
#else
		KString configFile = conf.path + "/webadmin";
#endif
		configFile += "/lang.xml";
		klog(KLOG_NOTICE, "load config file [%s]\n", configFile.c_str());
		klang.load(configFile.c_str());
	} catch (KXmlException& e) {
		fprintf(stderr, "%s\n", e.what());
	}
}
int do_config_thread(void* first_time, int argc) {
	assert(kfiber_self());
	if (first_time != NULL) {
		init_config();
		load_lang();
	}
	kconfig::reload();
	katom_set((void*)&load_config_progress, 0);
	return 0;
}
void do_config(bool first_time) {
	if (!katom_cas((void*)&load_config_progress, 0, 1)) {
		assert(!first_time);
		return;
	}
	assert(!kfiber_is_main());
	if (first_time) {
		do_config_thread((void*)&first_time, 0);
	} else {
		if (kfiber_create(do_config_thread, nullptr, 0, conf.fiber_stack_size, NULL) != 0) {
			katom_set((void*)&load_config_progress, 0);
		}
	}
}

void parse_server_software() {
	//Éú³ÉserverName
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
void wait_load_config_done() {
	for (;;) {
		if (0 == katom_get((void*)&load_config_progress)) {
			break;
		}
		kfiber_msleep(10);
	}
}
