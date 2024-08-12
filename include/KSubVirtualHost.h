/*
 * KSubVirtualHost.h
 *
 *  Created on: 2010-9-2
 *      Author: keengo
 */

#ifndef KSUBVIRTUALHOST_H_
#define KSUBVIRTUALHOST_H_
#include <string>
#include "KMark.h"
#include "KFileName.h"
#include "KHttpRequest.h"
#include "KVirtualHostContainer.h"
#include "krbtree.h"
#include "KHttpOpaque.h"
#include "KHtAccess.h"

enum class subdir_type : uint8_t
{
	subdir_local,
	subdir_http,
	subdir_server,
	subdir_portmap
};

class SubdirHttp
{
public:
	SubdirHttp() {
		memset(this, 0, sizeof(SubdirHttp));
		dst = new KUrl;
	}
	~SubdirHttp() {
		dst->release();
	}
	KUrl* dst;
	char* ip;
#ifdef ENABLE_UPSTREAM_SSL
	char* ssl;
#endif
	int life_time;
};
class SubdirServer
{
public:
	SubdirServer() {
		memset(this, 0, sizeof(SubdirServer));
	}
	~SubdirServer() {
		if (http_proxy) {
			xfree(http_proxy);
		}
	}
	char* http_proxy;
	char* https_proxy;
};
iterator_ret subdir_port_map_destroy(void* data, void* argv);
struct subdir_port_map_item
{
	int port;
	char* proxy;
};
struct SubdirPortMap
{
public:
	SubdirPortMap() {
		tree = rbtree_create();
	}
	~SubdirPortMap() {
		destroy();
		rbtree_destroy(tree);
	}
	void add(int port, const char* proxy);
	const char* find(int port);
private:
	void destroy() {
		rbtree_iterator(tree, subdir_port_map_destroy, NULL);
	}
	krb_tree* tree;
};
class KVirtualHost;


class KSubVirtualHost : public KHttpOpaque
{
public:
	KSubVirtualHost(KVirtualHost* vh);
	void setDocRoot(const char* doc_root, const char* dir);
#ifdef ENABLE_SVH_SSL
	bool set_ssl_info(const char* crt, const char* key, bool ssl_from_ext);
#endif
	void release() override;
	bool match_host(const char* host);
	bool setHost(const char* host);
	/**
	* ���url�������ļ���ת��
	* exsit��ʶ�ļ��Ƿ����
	*/
	kgl_jump_type bindFile(KHttpRequest* rq, KHttpObject* obj, bool& exsit, KApacheHtaccessContext& htctx, KSafeSource& fo);
	bool bindFile(KHttpRequest* rq, bool& exsit, bool searchDefaultFile, bool searchAlias);
	char* mapFile(const char* path);
	void free_subtype_data();
	char* host;
	KVirtualHost* vh;
	domain_t bind_host;
	bool wide;
	bool ssl_from_ext;
	subdir_type type;
#ifdef ENABLE_SVH_SSL
	kgl_ssl_ctx* ssl_ctx;
	char* ssl_param;
#endif
	char* dir;
	union
	{
		char* doc_root;
		SubdirHttp* http;
		SubdirServer* server;
		SubdirPortMap* portmap;
	};
	friend class KVirtualHost;
private:
	~KSubVirtualHost();
	KApacheHtaccessContext make_htaccess(const char* prefix, KFileName* file);
};
#endif /* KSUBVIRTUALHOST_H_ */
