#include "KCdnContainer.h"
#include "KHttpProxyFetchObject.h"
#include "KMultiAcserver.h"
#include "KAcserverManager.h"
#ifdef ENABLE_TCMALLOC
#include "google/heap-checker.h"
#endif
KCdnContainer* server_container = NULL;
using namespace std;
static int redirect_node_cmp(const void* k1, const void* k2) {
	const char* name = (const char*)k1;
	const KRedirectNode* rd = (const KRedirectNode*)k2;
	return strcasecmp(name, rd->name);
}
KCdnContainer::KCdnContainer() {
#ifdef ENABLE_TCMALLOC
	HeapLeakChecker::Disabler disabler;
#endif
	memset(&rd_list, 0, sizeof(rd_list));
	klist_init(&rd_list);
	rd_map = rbtree_create();
}
KCdnContainer::~KCdnContainer() {
	Clean();
	rbtree_destroy(rd_map);
}
KRedirect* KCdnContainer::refsRedirect(const char* ip, const char* host, int port, const char* ssl, int life_time, Proto_t proto) {
	KStringBuf s;
	s << "s://";
	if (ip) {
		s << ip << "_";
	}
	if (host) {
		s << host << ":";
	}
	s << port;
	if (ssl) {
		s << ssl;
	}
	s << "_" << (int)proto;
	lock.Lock();
	KRedirect* rd = findRedirect(s.getString());
	if (rd) {
		rd->addRef();
		lock.Unlock();
		return rd;
	}
	KSingleAcserver* server = new KSingleAcserver;
	server->set_proto(proto);
	server->sockHelper->setHostPort(host, port, ssl);
	server->sockHelper->setLifeTime(life_time);
	server->sockHelper->setIp(ip);
	KRedirectNode* rn = new KRedirectNode;
	rn->name = s.stealString();
	rn->rd = server;
	addRedirect(rn);
	server->addRef();
	lock.Unlock();
	return server;
}
KMultiAcserver* KCdnContainer::refsMultiServer(const char* name) {
	if (*name == '$') {
		return conf.gam->refsMultiAcserver(name + 1);
	}
	lock.Lock();
	KRedirect* rd = findRedirect(name);
	if (rd) {
		rd->addRef();
	}
	lock.Unlock();
	if (rd == NULL) {
		return NULL;
	}
	if (strcmp(rd->getType(), "mserver") != 0) {
		rd->release();
		return NULL;
	}
	return static_cast<KMultiAcserver*>(rd);
}
KRedirect* KCdnContainer::refsRedirect(const char* name) {
	if (*name == '$') {
		return conf.gam->refsMultiAcserver(name + 1);
	}
	lock.Lock();
	KRedirect* rd = findRedirect(name);
	if (rd) {
		rd->addRef();
		lock.Unlock();
		return rd;
	}
	char* buf = strdup(name);
	KSockPoolHelper* nodes = NULL;
	Proto_t proto = Proto_http;
	bool ip_hash = false;
	bool url_hash = false;
	bool cookie_stick = false;
	int max_error_count = 3;
	int error_try_time = 0;
	const char* icp = NULL;
	char* hot = buf;
	for (;;) {
		char* p = strchr(hot, '/');
		if (p) {
			*p = '\0';
		}
		char* eq = strchr(hot, '=');

		if (eq) {
			*eq = '\0';
			char* val = eq + 1;
			if (strcasecmp(hot, "proto") == 0) {
				proto = KPoolableRedirect::parseProto(val);
			} else if (strcasecmp(hot, "ip_hash") == 0) {
				ip_hash = atoi(val) > 0;
			} else if (strcasecmp(hot, "url_hash") == 0) {
				url_hash = atoi(val) > 0;
			} else if (strcasecmp(hot, "cookie_stick") == 0) {
				cookie_stick = atoi(val) > 0;
			} else if (strcasecmp(hot, "error_try_time") == 0) {
				error_try_time = atoi(val);
				//{{ent
#ifdef ENABLE_MSERVER_ICP
			} else if (strcasecmp(hot, "icp") == 0) {
				icp = val;
#endif//}}
			} else if (strcasecmp(hot, "error_count") == 0) {
				max_error_count = atoi(val);
			} else if (strcasecmp(hot, "nodes") == 0) {
				if (nodes == NULL) {
					nodes = KPoolableRedirect::parse_nodes(val);
				}
			}
		}
		if (p == NULL) {
			break;
		}
		hot = p + 1;
	}
	if (nodes != NULL) {
		if (nodes->next
			//{{ent
#ifdef ENABLE_MSERVER_ICP
			|| icp != NULL
#endif//}}
			) {
			KMultiAcserver* server = new KMultiAcserver(nodes);
			//{{ent
#ifdef ENABLE_MSERVER_ICP
			if (icp != NULL) {
				server->setIcp(icp);
			}
#endif//}}
			server->setErrorTryTime(max_error_count, error_try_time);
			server->set_proto(proto);
			server->ip_hash = ip_hash;
			server->url_hash = url_hash;
			server->cookie_stick = cookie_stick;
			rd = server;
		} else {
			KSingleAcserver* server = new KSingleAcserver(nodes);
			server->set_proto(proto);
			rd = server;
		}
	}
	free(buf);
	if (rd != NULL) {
		KRedirectNode* rn = new KRedirectNode;
		rn->name = strdup(name);
		rn->rd = rd;
		addRedirect(rn);
		rd->addRef();
	}
	lock.Unlock();
	return rd;
}
KFetchObject* KCdnContainer::get(const char* ip, const char* host, int port, const char* ssl, int life_time, Proto_t proto) {
	KRedirect* server = refsRedirect(ip, host, port, ssl, life_time, proto);
	KBaseRedirect* brd = new KBaseRedirect(server, KGL_CONFIRM_FILE_NEVER);
	KRedirectSource* fo = new KHttpProxyFetchObject();
	fo->bind_base_redirect(brd);
	return fo;
}
KFetchObject* KCdnContainer::get(const char* name) {
	KRedirect* server = refsRedirect(name);
	if (server == NULL) {
		return NULL;
	}
	KBaseRedirect* brd = new KBaseRedirect(server, KGL_CONFIRM_FILE_NEVER);
	KRedirectSource* fo = new KHttpProxyFetchObject();
	fo->bind_base_redirect(brd);
	return fo;
}
void KCdnContainer::Clean() {
	lock.Lock();
	KRedirectNode* rn = rd_list.next;
	while (rn != &rd_list) {
		KRedirectNode* next = rn->next;
		Remove(rn);
		rn = next;
	}
	lock.Unlock();
}
void KCdnContainer::flush(time_t nowTime) {
	lock.Lock();
	KRedirectNode* rn = rd_list.next;
	while (rn != &rd_list) {
		if (nowTime - rn->lastActive < 300) {
			break;
		}
		KRedirectNode* next = rn->next;
		Remove(rn);
		rn = next;
	}
	lock.Unlock();
}
void KCdnContainer::Remove(KRedirectNode* rn) {
	rn->rd->release();
	klist_remove(rn);
	rbtree_remove(rd_map, rn->node);
	xfree(rn->name);
	delete rn;
}
KRedirect* KCdnContainer::findRedirect(const char* name) {
	krb_node* node = rbtree_find(rd_map, (void*)name, redirect_node_cmp);
	if (node == NULL) {
		return NULL;
	}
	KRedirectNode* rn = (KRedirectNode*)node->data;
	rn->lastActive = kgl_current_sec;
	klist_remove(rn);
	klist_append(&rd_list, rn);
	return rn->rd;
}
void KCdnContainer::addRedirect(KRedirectNode* rn) {
	int new_flags = 0;
	rn->node = rbtree_insert(rd_map, (void*)rn->name, &new_flags, redirect_node_cmp);
	assert(new_flags);
	rn->lastActive = kgl_current_sec;
	klist_append(&rd_list, rn);
	rn->node->data = rn;
}
