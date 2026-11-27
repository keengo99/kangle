/*
 * KSubVirtualHost.cpp
 *
 *  Created on: 2010-9-2
 *      Author: keengo
 */
#include <vector>
#include <string.h>
#include <sstream>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <iterator>
#include "KVirtualHost.h"
#include "KSubVirtualHost.h"
#include "KHtAccess.h"
#include "http.h"
#include "KCdnContainer.h"
#include "KHttpProxyFetchObject.h"
#include "kmalloc.h"
#include "HttpFiber.h"
#include "KDefer.h"
#include "KAcserverManager.h"
std::string htaccess_filename;
using namespace std;
iterator_ret subdir_port_map_destroy(void* data, void* argv) {
	subdir_port_map_item* item = (subdir_port_map_item*)data;
	free(item->proxy);
	delete item;
	return iterator_remove_continue;
}
void SubdirPortMap::add(int port, const char* proxy) {
	int new_flag = 0;
	krb_node* node = rbtree_insert(tree, &port, &new_flag, rbtree_int_cmp);
	if (new_flag == 0) {
		return;
	}
	subdir_port_map_item* item = new subdir_port_map_item;
	item->port = port;
	item->proxy = (char*)strdup(proxy);
	node->data = item;
}
const char* SubdirPortMap::find(int port) {
	krb_node* node = rbtree_find(tree, &port, rbtree_int_cmp);
	if (node) {
		subdir_port_map_item* item = (subdir_port_map_item*)node->data;
		return item->proxy;
	}
	return NULL;
}
KSubVirtualHost::KSubVirtualHost(KVirtualHost* vh) {
	this->vh = vh;
	host = NULL;
	dir = NULL;
	doc_root = NULL;
	type = subdir_type::subdir_local;
	bind_host = NULL;
	wide = false;
#ifdef ENABLE_SVH_SSL
	ssl_ctx = NULL;
	ssl_param = NULL;
#endif
}
KSubVirtualHost::~KSubVirtualHost() {
	if (host) {
		xfree(host);
	}
	if (bind_host) {
		xfree(bind_host);
	}
	if (dir) {
		xfree(dir);
	}
	free_subtype_data();
#ifdef ENABLE_SVH_SSL
	if (ssl_ctx) {
		kgl_release_ssl_ctx(ssl_ctx);
	}
	if (ssl_param) {
		free(ssl_param);
	}
#endif
}
void KSubVirtualHost::free_subtype_data() {
	if (doc_root == NULL) {
		return;
	}
	switch (type) {
	case subdir_type::subdir_http:
		delete http;
		break;
	case subdir_type::subdir_server:
		delete server;
		break;
	case subdir_type::subdir_portmap:
		delete portmap;
		break;
	case subdir_type::subdir_redirect:
		delete sub_rd;
		break;
	default:
		xfree(doc_root);
	}
	doc_root = NULL;
}
void KSubVirtualHost::release() {
	vh->release();
}
#ifdef ENABLE_SVH_SSL
bool KSubVirtualHost::set_ssl_info(const char* crt, const char* key, bool ssl_from_ext) {
	KString certfile, keyfile;
	if (*crt == '-') {
		certfile = conf.path + (crt + 1);
	} else if (!isAbsolutePath(crt)) {
		certfile = vh->doc_root + crt;
	} else {
		certfile = crt;
	}
	if (*key == '-') {
		keyfile = conf.path + (key + 1);
	} else if (!isAbsolutePath(key)) {
		keyfile = vh->doc_root + key;
	} else {
		keyfile = key;
	}
	if (ssl_ctx) {
		kgl_release_ssl_ctx(ssl_ctx);
		ssl_ctx = nullptr;
	}
	ssl_ctx = vh->new_ssl_ctx(certfile, keyfile);
	if (!ssl_ctx) {
		return false;
	}
	if (vh->ssl_ctx == NULL) {
		vh->cert_file = crt;
		vh->key_file = key;
	}
	return true;
}
#endif
bool KSubVirtualHost::match_host(const char* host) {
	bool wide = false;
	if (*host == '*') {
		host++;
		wide = true;
	}
	if (*host == '.') {
		wide = true;
	}
	if (this->wide != wide) {
		return false;
	}
	return strcasecmp(this->host, host) == 0;
}
bool KSubVirtualHost::setHost(const char* host) {
	wide = false;
	if (bind_host) {
		xfree(bind_host);
		bind_host = NULL;
	}
	if (*host == '*') {
		host++;
		wide = true;
	}
	if (*host == '.') {
		wide = true;
	}
	assert(this->host == NULL);
	this->host = xstrdup(host);
	char* p = strchr(this->host, '|');
	if (p) {
		*p = '\0';
		this->dir = strdup(p + 1);
	}
	int bind_host_len = (int)strlen(this->host);
	this->bind_host = (domain_t)malloc(bind_host_len + 2);
	bool result = revert_hostname(this->host, bind_host_len, this->bind_host, bind_host_len + 2);
	if (!result) {
		klog(KLOG_ERR, "bind hostname [%s] is error\n", this->host);
	}
	return result;
}
/* A directory embedded in setHost (after '|') takes precedence. */
void KSubVirtualHost::set_doc_root(const char* doc_root, const char* dir) {
	free_subtype_data();
	if (this->dir == nullptr) {
		if (dir == nullptr) {
			this->dir = xstrdup("/");
		} else {
			this->dir = xstrdup(dir);
		}
	}
	char* ssl_crt = strchr(this->dir, KGL_SSL_PARAM_SPLIT_CHAR);
	if (ssl_crt != nullptr) {
		*ssl_crt = '\0';
#ifdef ENABLE_SVH_SSL
		ssl_crt++;
		ssl_param = strdup(ssl_crt);
		char* ssl_key = strchr(ssl_crt, '|');
		if (ssl_key != NULL) {
			*ssl_key = '\0';
			ssl_key++;
			set_ssl_info(ssl_crt, ssl_key, false);
		}
#endif
	}
	//subdir_http
	if (strncasecmp(this->dir, "http://", 7) == 0) {
		http = new SubdirHttp;
		type = subdir_type::subdir_http;
		if (!parse_upstream_url(this->dir, http->dst)) {
			free_subtype_data();
			klog(KLOG_ERR, "cann't parse url [%s]\n", this->dir);
			return;
		}
		/* Same default as an HTTP <server> without life_time;
		 * an explicit life_time=0 still disables reuse. */
		http->life_time = KGL_HTTP_UPSTREAM_LIFE_TIME;
		if (http->dst->param) {
			char* t = strstr(http->dst->param, "life_time=");
			if (t) {
				http->life_time = atoi(t + 10);
			}
			http->ip = strstr(http->dst->param, "ip=");
			//{{ent
#ifdef ENABLE_UPSTREAM_SSL
			http->ssl = strstr(http->dst->param, "ssl=");
			if (http->ssl) {
				http->ssl += 4;
				t = strchr(http->ssl, '&');
				if (t) {
					*t = '\0';
				}
			}
#endif//}}
			if (http->ip) {
				http->ip += 3;
				char* t = strchr(http->ip, '&');
				if (t) {
					*t = '\0';
				}
			}
		}
		return;
	}
	//subdir_redirect
	if (strncasecmp(this->dir, "rd://", 5) == 0) {
		type = subdir_type::subdir_redirect;
		sub_rd = new SubdirRedirect;
		char* rd_str = strdup(this->dir + 5);
		char* rd_ssl = nullptr;
		char* p = strchr(rd_str, '|');
		if (p) {
			*p = '\0';
			rd_ssl = p + 1;
		}
		sub_rd->http_rd = conf.gam->refsAcserver(rd_str);
		if (rd_ssl) {
			sub_rd->https_rd = conf.gam->refsAcserver(rd_ssl);
		} else if (sub_rd->http_rd) {
			sub_rd->https_rd = static_cast<KRedirect*>(sub_rd->http_rd->add_ref());
		}
		free(rd_str);
		return;
	}
	//subdir_server
	if (strncasecmp(this->dir, "server://", 9) == 0) {
		type = subdir_type::subdir_server;
		server = new SubdirServer;
		server->http_proxy = strdup(this->dir + 9);
		char* p = strchr(server->http_proxy, '|');
		if (p) {
			*p = '\0';
			server->https_proxy = p + 1;
		}
		return;
	}
	if (strncmp(this->dir, "portmap://", 10) == 0) {
		type = subdir_type::subdir_portmap;
		portmap = new SubdirPortMap;
		char* buf = strdup(this->dir + 10);
		char* hot = buf;
		for (;;) {
			char* p = strchr(hot, '|');
			if (p) {
				*p = '\0';
			}
			char* eq = strchr(hot, '=');
			if (eq) {
				*eq = '\0';
				portmap->add(atoi(hot), eq + 1);
			}
			if (p == NULL) {
				break;
			}
			hot = p + 1;
		}
		free(buf);
		return;
	}
	//subdir_local
	type = subdir_type::subdir_local;
	KFileName::tripDir3(this->dir, '/');
	char* sub_doc_root = KFileName::concatDir(doc_root, this->dir);
	if (sub_doc_root == NULL) {
		this->doc_root = NULL;
		return;
	}
	this->doc_root = sub_doc_root;
	size_t doc_len = strlen(this->doc_root);
	if (this->doc_root[doc_len - 1] != '/'
#ifdef _WIN32
		&& this->doc_root[doc_len - 1] != '\\'
#endif
		) {
		sub_doc_root = (char*)xmalloc(doc_len + 2);
		kgl_memcpy(sub_doc_root, this->doc_root, doc_len);
		sub_doc_root[doc_len] = PATH_SPLIT_CHAR;
		sub_doc_root[doc_len + 1] = '\0';
		xfree(this->doc_root);
		this->doc_root = sub_doc_root;
	}
	KFileName::tripDir3(this->doc_root, PATH_SPLIT_CHAR);
}
kgl_jump_type KSubVirtualHost::bindFile(KHttpRequest* rq, KHttpObject* obj, bool& exsit, KApacheHtaccessContext& htctx, KSafeSource& fo) {
#ifdef _WIN32
	const char* url_path = rq->sink->data.url->path;
	if (url_path && *url_path) {
		char* c = (char*)url_path + strlen(url_path) - 1;
		if (*c == '.' || *c == ' ') {
			return JUMP_DENY;
		}
	}
#endif
	if (doc_root == NULL) {
		return JUMP_DENY;
	}
	KStringBuf tmp_str;
	const char* proxy = NULL;
	switch (type) {
	case subdir_type::subdir_local:
	{
		if (!rq->ctx.internal && !vh->htaccess.empty()) {
			kgl_auto_cstr path(xstrdup(rq->sink->data.url->path));
			int prefix_len = 0;
			for (;;) {
				char* hot = strrchr(path.get(), '/');
				if (hot == NULL) {
					break;
				}
				if (prefix_len == 0) {
					prefix_len = (int)(hot - path.get());
				}
				*hot = '\0';
				kgl_auto_cstr apath = vh->alias(rq->ctx.internal, path.get());
				KFileName htfile;
				bool htfile_exsit = false;
				if (apath) {
					htfile_exsit = htfile.setName(apath.get(), vh->htaccess.c_str(), 0);
				} else {
					stringstream s;
					s << doc_root << path.get();
					htfile_exsit = htfile.setName(s.str().c_str(), vh->htaccess.c_str(), 0);
				}
				if (htfile_exsit) {
					htctx = make_htaccess(path.get(), &htfile);
					if (htctx) {
						int jump_type = htctx->access[REQUEST]->check(rq, obj, fo);
						if (fo || jump_type == JUMP_DENY) {
							htctx = nullptr;
							return jump_type;
						}
					}
				}
			}
		}
		if (rq->file) {
			// Rebind the file because a rewrite may have changed the path.
			delete rq->file;
			rq->file = NULL;
		}
		bindFile(rq, exsit, false, true);
		return JUMP_ALLOW;
	}
	case subdir_type::subdir_http:
	{
		if (http->dst->host == NULL) {
			return JUMP_DENY;
		}
		if (*(http->dst->host) == '-') {
			fo.reset(new KHttpProxyFetchObject());
			return JUMP_ALLOW;
		}
		const char* tssl = NULL;
#ifdef ENABLE_UPSTREAM_SSL
		tssl = http->ssl;
#endif
		int tport = http->dst->port;
		if (http->dst->port == 0) {
			tport = rq->sink->data.url->port;
#ifdef ENABLE_UPSTREAM_SSL
			if (KBIT_TEST(rq->sink->data.url->flags, KGL_URL_SSL) && tssl == NULL) {
				tssl = "s";
			}
#endif
		}
		fo.reset(server_container->get(http->ip, http->dst->host, tport, tssl, http->life_time));
		return JUMP_ALLOW;
	}
	case subdir_type::subdir_redirect:
	{
		KRedirect* rd = nullptr;
		if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_SSL)) {
			if (sub_rd->https_rd) {
				rd = static_cast<KRedirect*>(sub_rd->https_rd->add_ref());
			}
		} else {
			if (sub_rd->http_rd) {
				rd = static_cast<KRedirect*>(sub_rd->http_rd->add_ref());
			}
		}
		if (!rd) {
			return JUMP_DENY;
		}
		KRedirectSource* fo2 = rd->makeFetchObject(rq, rq->file);
		KBaseRedirect* brd = new KBaseRedirect(rd, KConfirmFile::Never);
		fo2->bind_base_redirect(brd);
		fo.reset(fo2);
		return JUMP_ALLOW;
	}
	case subdir_type::subdir_server:
	{
		proxy = server->http_proxy;
		if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_SSL) && server->https_proxy) {
			proxy = server->https_proxy;
		}
		break;
	}
	case subdir_type::subdir_portmap:
	{
		int port = (int)rq->GetSelfPort();
		if (port == 0) {
			port = (int)rq->sink->data.url->port;
		}
		proxy = portmap->find(port);
		break;
	}
	}
	if (proxy == NULL) {
		return JUMP_DENY;
	}
	if (*proxy == '/') {
		/*
		* upstream is ssl,so same host:port may be not support connection reuse.
		* sni affect different host use different connection.
		*/
		tmp_str << rq->sink->data.url->host << proxy;
		proxy = tmp_str.c_str();
	}
	KRedirect* rd = server_container->refsRedirect(proxy);
	if (rd == NULL) {
		return JUMP_DENY;
	}
	KRedirectSource* fo2 = rd->makeFetchObject(rq, rq->file);
	KBaseRedirect* brd = new KBaseRedirect(rd, KConfirmFile::Never);
	fo2->bind_base_redirect(brd);
	fo.reset(fo2);
	return JUMP_ALLOW;
}
bool KSubVirtualHost::bindFile(KHttpRequest* rq, bool& exsit, bool searchDefaultFile, bool searchAlias) {
	KFileName* file = new KFileName;
	if (!searchAlias || !vh->alias(rq->ctx.internal, rq->sink->data.url->path, file, exsit, rq->getFollowLink())) {
		exsit = file->setName(doc_root, rq->sink->data.url->path, rq->getFollowLink());
	}
	kassert(rq->file == NULL);
	rq->file = file;
	if (searchDefaultFile && file->isDirectory()) {
		KFileName* defaultFile = NULL;
		if (vh->getIndexFile(rq, file, &defaultFile, NULL)) {
			delete rq->file;
			rq->file = defaultFile;
		}
	}
	return true;
}
namespace {
	/*
	 * Parsed .htaccess files. Parsing compiles every regex, which costs
	 * milliseconds for a large file, so requests reuse the result until the
	 * file changes.
	 */
	struct KHtaccessCacheEntry
	{
		KApacheHtaccessContext ctx;
		int64_t size;
		time_t mtime;
		time_t ctime;
		long mtime_nsec;
		uint64_t ino;
		uint64_t dev;
		time_t last_used;
	};
	constexpr int64_t htaccess_cache_max_memory = 64 * 1024 * 1024;
	constexpr time_t htaccess_cache_idle = 300;
	constexpr time_t htaccess_cache_sweep_interval = 60;
	std::mutex htaccess_cache_lock;
	// Never freed, so destroying contexts does not happen during process exit.
	std::unordered_map<std::string, KHtaccessCacheEntry>* htaccess_cache = new std::unordered_map<std::string, KHtaccessCacheEntry>;
	int64_t htaccess_cache_memory = 0;
	time_t htaccess_cache_swept = 0;

	// Estimated memory of a parsed file, measured on Linux: about 16KB for the
	// access objects plus 90 times the file size, mostly JIT-compiled regex code.
	int64_t htaccess_memory(int64_t size) {
		return 16384 + size * 90;
	}
	long stat_mtime_nsec(const struct _stat64* st) {
#ifdef __linux__
		return st->st_mtim.tv_nsec;
#else
		return 0;
#endif
	}
	bool htaccess_cache_match(const KHtaccessCacheEntry& e, const struct _stat64* st) {
		// ctime also changes on chmod, so a file that could not be read is retried.
		return e.size == (int64_t)st->st_size && e.mtime == st->st_mtime && e.ctime == st->st_ctime
			&& e.mtime_nsec == stat_mtime_nsec(st) && e.ino == (uint64_t)st->st_ino && e.dev == (uint64_t)st->st_dev;
	}
	// The functions below are called with htaccess_cache_lock held. Contexts are
	// moved to dropped so they are destroyed after the lock is released.
	void htaccess_cache_erase(std::unordered_map<std::string, KHtaccessCacheEntry>::iterator it, std::vector<KApacheHtaccessContext>& dropped) {
		htaccess_cache_memory -= htaccess_memory(it->second.size);
		dropped.push_back(std::move(it->second.ctx));
		htaccess_cache->erase(it);
	}
	// Make room for a new entry that needs memory bytes.
	void htaccess_cache_sweep(time_t now, int64_t memory, std::vector<KApacheHtaccessContext>& dropped) {
		bool full = htaccess_cache_memory + memory > htaccess_cache_max_memory;
		if (!full && now - htaccess_cache_swept < htaccess_cache_sweep_interval) {
			return;
		}
		htaccess_cache_swept = now;
		for (auto it = htaccess_cache->begin(); it != htaccess_cache->end();) {
			auto next = std::next(it);
			if (now - it->second.last_used > htaccess_cache_idle) {
				htaccess_cache_erase(it, dropped);
			}
			it = next;
		}
		if (htaccess_cache_memory + memory <= htaccess_cache_max_memory) {
			return;
		}
		// Still full: drop the least recently used entries down to 3/4 of the limit.
		std::vector<std::pair<time_t, std::string>> order;
		order.reserve(htaccess_cache->size());
		for (auto& it : *htaccess_cache) {
			order.emplace_back(it.second.last_used, it.first);
		}
		std::sort(order.begin(), order.end());
		for (auto& o : order) {
			if (htaccess_cache_memory + memory <= htaccess_cache_max_memory * 3 / 4) {
				break;
			}
			htaccess_cache_erase(htaccess_cache->find(o.second), dropped);
		}
	}
}
KApacheHtaccessContext KSubVirtualHost::make_htaccess(const char* prefix, KFileName* file) {
	// file was found with lstat and symlinks refused, so this is the file itself.
	const struct _stat64* st = file->get_stat();
	std::string key(file->getName());
	key.push_back('\0');
	key.append(prefix);
	time_t now = kgl_current_sec;
	std::vector<KApacheHtaccessContext> dropped;
	{
		std::lock_guard<std::mutex> guard(htaccess_cache_lock);
		auto it = htaccess_cache->find(key);
		if (it != htaccess_cache->end()) {
			if (htaccess_cache_match(it->second, st)) {
				it->second.last_used = now;
				return it->second.ctx;
			}
			htaccess_cache_erase(it, dropped);
		}
	}
	dropped.clear();
	KApacheHtaccessContext ctx(new _KApacheHtaccessContext(file->getName(), prefix));
	try {
		ctx->file->reload(true);
	} catch (std::exception& e) {
		klog(KLOG_ERR, "load htaccess file error [%s]\n", e.what());
	}
	// A second edit within the same second may keep the same mtime and size,
	// so a file that changed just now is not cached yet.
	time_t changed = st->st_mtime > st->st_ctime ? st->st_mtime : st->st_ctime;
	int64_t memory = htaccess_memory((int64_t)st->st_size);
	if (now - changed < 2 || memory > htaccess_cache_max_memory / 4) {
		return ctx;
	}
	{
		std::lock_guard<std::mutex> guard(htaccess_cache_lock);
		// another request may have cached the same file meanwhile
		auto it = htaccess_cache->find(key);
		if (it != htaccess_cache->end()) {
			htaccess_cache_erase(it, dropped);
		}
		htaccess_cache_sweep(now, memory, dropped);
		auto& e = (*htaccess_cache)[key];
		htaccess_cache_memory += memory;
		e.ctx = ctx;
		e.size = (int64_t)st->st_size;
		e.mtime = st->st_mtime;
		e.ctime = st->st_ctime;
		e.mtime_nsec = stat_mtime_nsec(st);
		e.ino = (uint64_t)st->st_ino;
		e.dev = (uint64_t)st->st_dev;
		e.last_used = now;
	}
	return ctx;
}
kgl_auto_cstr KSubVirtualHost::map_file(const char* path) {
	kgl_auto_cstr new_path = vh->alias(true, path);
	if (new_path) {
		return new_path;
	}
	return kgl_auto_cstr(KFileName::concatDir(doc_root, path));
}
