/*
 * KSubVirtualHost.cpp
 *
 *  Created on: 2010-9-2
 *      Author: keengo
 */
#include <vector>
#include <string.h>
#include <sstream>
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
	}
	if (*key == '-') {
		keyfile = conf.path + (key + 1);
	} else if (!isAbsolutePath(key)) {
		keyfile = vh->doc_root + key;
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
/* 如果setHost里面设置了dir信息(|分隔),以setHost的为准 */
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
		if (!parse_url(this->dir, http->dst)) {
			free_subtype_data();
			klog(KLOG_ERR, "cann't parse url [%s]\n", this->dir);
			return;
		}
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
	int path_len = (int)strlen(rq->sink->data.url->path);
	char* c = rq->sink->data.url->path + path_len - 1;
	if (*c == '.' || *c == ' ') {
		return JUMP_DENY;
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
			//重新绑定过,因为有可能重写了
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
		if (KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_SSL)) {
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
		if (KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_SSL) && server->https_proxy) {
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
KApacheHtaccessContext KSubVirtualHost::make_htaccess(const char* prefix, KFileName* file) {
	KApacheHtaccessContext ctx(new _KApacheHtaccessContext(file->getName()));
	try {
		ctx->file->reload(true);
	} catch (std::exception& e) {
		klog(KLOG_ERR, "load htaccess file error [%s]\n", e.what());
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

