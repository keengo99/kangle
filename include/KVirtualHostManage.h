/*
 * Kconf.gvm->h
 *
 *  Created on: 2010-4-19
 *      Author: keengo
 */
#ifndef KVIRTUALHOSTMANAGE_H_
#define KVIRTUALHOSTMANAGE_H_
#include <map>
#include <vector>
#include "global.h"
#define VH_CONFIG_FILE	"/etc/vh.xml"
#include "KXmlSupport.h"
#include "KVirtualHost.h"
#include "KMutex.h"
#include "KDynamicListen.h"
class KGTempleteVirtualHost;
void on_vh_manage_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
class KVirtualHostManage : public kconfig::KConfigListen
{
public:
	KVirtualHostManage();
	virtual ~KVirtualHostManage();
	void getMenuHtml(KWStream& s, KVirtualHost* vh, KStringBuf& url, int t);
	void getHtml(KWStream& s, const KString& vh_name, int id, KUrlValue& attribute);
	void GetListenHtml(KWStream& s);
	void GetListenWhm(WhmContext* ctx);
	void shutdown();
	int getNextInstanceId();
	void getAutoName(KString& name);
	void BindGlobalListen(KListenHost* services);
	void UnBindGlobalListen(KListenHost* services);
	void BindGlobalListens(std::vector<KListenHost*>& services);
	void UnBindGlobalListens(std::vector<KListenHost*>& services);
	bool BindSsl(const char* domain, const char* cert_file, const char* key_file);
	bool on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) override;
	kconfig::KConfigEventFlag config_flag() const override {
		return kconfig::ev_subdir | kconfig::ev_skip_vary;
	}
	int getCount();
public:
	/*
	 * 更新虚拟主机
	 */
	bool updateVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh);
	bool updateVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh, KVirtualHost* ov);
	void updateVirtualHost(KVirtualHost* vh, std::list<KSubVirtualHost*>& hosts);
	void updateVirtualHost(KVirtualHost* vh, std::list<KString>& binds);
	/*
	 * 增加虚拟主机
	 */
	bool addVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh);
	/*
	 * 删除虚拟主机
	 */
	bool removeVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh);
	static query_vh_result queryVirtualHost(KVirtualHostContainer* vhc, KSubVirtualHost** rq_svh, const char* site, int site_len);
	int find_domain(const char* domain, WhmContext* ctx);
	void getAllVh(std::list<KString>& vhs, bool status, bool onlydb);
	KVirtualHost* refsVirtualHostByName(const KString& name);
	kserver* RefsServer(u_short port);
#ifdef ENABLE_VH_FLOW
	void dumpLoad(KVirtualHostEvent* ctx, bool revers, const char* prefix, int prefix_len);
	//导出所有虚拟主机流量
	void dumpFlow();
	void dumpFlow(KVirtualHostEvent* ctx, bool revers, const char* prefix, int prefix_len, int extend);
#endif
	KBaseVirtualHost vhs;
public:
	friend class KHttpServerParser;
	friend class KHttpManage;
	friend class KDynamicListen;
	static KDynamicListen* get_listen() {
		return &dlisten;
	}
	static KFiberLocker locker() {
		return KFiberLocker(lock);
	}
private:
	void getVhIndex(KWStream& s, KVirtualHost* vh, int id, int t);
	bool internalAddVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh, KVirtualHost* ov);
	bool internalRemoveVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh);
	void getAllVhHtml(KWStream& s, int tvh);
	void getVhDetail(KWStream& s, KVirtualHost* vh, bool edit, int t);
	/*
	 * 所有虚拟主机列表
	 */
	std::map<KString, KVirtualHost*> avh;
	/*
	* 绑定到侦听上
	*/
	void InternalBindVirtualHost(KVirtualHost* vh);
	void InternalUnBindVirtualHost(KVirtualHost* vh);
	/*
	* 查找全局虚拟主机并绑定上。
	*/
	void BindGlobalVirtualHost(kserver* server);
	void UnBindGlobalVirtualHost(kserver* server);

#ifdef ENABLE_SVH_SSL
	KDomainMap* ssl_config;
#endif
private:
	static kfiber_mutex* lock;
	static KDynamicListen dlisten;
};
#endif /* KVIRTUALHOSTMANAGE_H_ */
