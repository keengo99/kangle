/*
 * KSubVirtualHost.h
 *
 *  Created on: 2010-9-2
 *      Author: keengo
 */

#ifndef KSUBVIRTUALHOST_H_
#define KSUBVIRTUALHOST_H_
#include <string>
#include "KFileName.h"
#include "KHttpRequest.h"
#include "KVirtualHostContainer.h"
#include "krbtree.h"

enum subdir_type
{
	subdir_local,
	subdir_http,
	subdir_server,
	subdir_portmap
};

class SubdirHttp
{
public:
	SubdirHttp()
	{
		memset(this, 0, sizeof(SubdirHttp));
	}
	~SubdirHttp() {
	}
	KUrl dst;
	char *ip;
	//{{ent
#ifdef ENABLE_UPSTREAM_SSL
	char *ssl;
#endif//}}
	int lifeTime;
};
class SubdirServer
{
public:
	SubdirServer()
	{
		memset(this, 0, sizeof(SubdirServer));
	}
	~SubdirServer()
	{
		if (http_proxy) {
			xfree(http_proxy);
		}		
	}
	char *http_proxy;
	char *https_proxy;
};
iterator_ret subdir_port_map_destroy(void *data, void *argv);
struct subdir_port_map_item
{
	int port;
	char *proxy;
};
struct SubdirPortMap
{
public:
	SubdirPortMap()
	{
		tree = rbtree_create();
	}
	~SubdirPortMap()
	{
		destroy();
		rbtree_destroy(tree);
	}
	void add(int port, const char *proxy);
	const char *find(int port);
private:
	void destroy()
	{
		rbtree_iterator(tree, subdir_port_map_destroy, NULL);
	}
	krb_tree *tree;
};
class KVirtualHost;


class KSubVirtualHost {
public:
	KSubVirtualHost(KVirtualHost *vh);
	void setDocRoot(const char *doc_root,const char *dir);
#ifdef ENABLE_SVH_SSL
	bool SetSslInfo(const char *crt, const char *key);
#endif
	void release();
	bool MatchHost(const char *host);
	bool setHost(const char *host);
	/**
	* 完成url到物理文件的转换
	* exsit标识文件是否存在
	* htresponse htaccess转换后的access对象
	* handled 是否已经处理了rq,如htaccess已经发送数据给rq(重定向,拒绝等等)
	*/
	bool bindFile(KHttpRequest *rq,KHttpObject *obj,bool &exsit,KAccess **htresponse,bool &handled);
	bool bindFile(KHttpRequest *rq,bool &exsit,bool searchDefaultFile,bool searchAlias);
	char *mapFile(const char *path);
	void free_subtype_data();
	char *host;
	KVirtualHost *vh;
	domain_t bind_host;
	bool wide;
	bool allSuccess;
	bool fromTemplete;
	subdir_type type;
#ifdef ENABLE_SVH_SSL
	kgl_ssl_ctx *ssl_ctx;
	char *ssl_param;
#endif
	char *dir;
	union {
		char *doc_root;
		SubdirHttp *http;
		SubdirServer *server;
		SubdirPortMap *portmap;
	};
	friend class KVirtualHost;
private:
	~KSubVirtualHost();
	bool makeHtaccess(const char *prefix,KFileName *file,KAccess *request,KAccess *response);
};
#endif /* KSUBVIRTUALHOST_H_ */
