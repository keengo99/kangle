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
#include "KUrlValue.h"

class KGTempleteVirtualHost;

class KVirtualHostManage : public kconfig::KConfigListen
{
public:
	KVirtualHostManage();
	virtual ~KVirtualHostManage();
	void getMenuHtml(KWStream& s, KVirtualHost* vh, KStringBuf& url);
	void getHtml(KWStream& s, const KString& vh_name, int id, KUrlValue& attribute);
	bool vh_base_action(KUrlValue& attribute, KString& err_msg);
	void GetListenHtml(KWStream& s);
	void GetListenWhm(WhmContext* ctx);
	void shutdown();
	int getNextInstanceId();
	void getAutoName(KString& name);
	void BindGlobalListen(KListenHost* services);
	void UnBindGlobalListen(KListenHost* services);
	void BindGlobalListens(std::vector<KListenHost*>& services);
	void UnBindGlobalListens(std::vector<KListenHost*>& services);
#ifdef ENABLE_SVH_SSL
	bool add_ssl(const char* domain, const char* cert_file, const char* key_file);
	bool remove_ssl(const char* domain);
	bool update_ssl(const char* domain, const char* cert_file, const char* key_file);
#endif
	bool on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) override;
	kconfig::KConfigEventFlag config_flag() const override {
		return kconfig::ev_subdir | kconfig::ev_skip_vary;
	}
	int getCount();
public:
	/*
	 * ������������
	 */
	bool updateVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh);
	bool updateVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh, KVirtualHost* ov);
	//void updateVirtualHost(KVirtualHost* vh, std::list<KSubVirtualHost*>& hosts);
	void updateVirtualHost(KVirtualHost* vh, std::list<KString>& binds);
	/*
	 * ������������
	 */
	bool addVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh);
	/*
	 * ɾ����������
	 */
	bool removeVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh);
	static query_vh_result queryVirtualHost(KVirtualHostContainer* vhc, KSubVirtualHost** rq_svh, const char* site, int site_len);
	int find_domain(const char* domain, WhmContext* ctx);
	void getAllVh(std::list<KString>& vhs, bool status, bool onlydb);
	KVirtualHost* refsVirtualHostByName(const KString& name);
	kserver* RefsServer(u_short port);
#ifdef ENABLE_VH_FLOW
	void dumpLoad(KVirtualHostEvent* ctx, bool revers, const char* prefix, int prefix_len);
	//��������������������
	void dumpFlow();
	void dumpFlow(KVirtualHostEvent* ctx, bool revers, const char* prefix, int prefix_len, int extend);
#endif
	KBaseVirtualHost vhs;
public:
	friend class KHttpManage;
	friend class KDynamicListen;
	static KDynamicListen* get_listen() {
		return &dlisten;
	}
	static KFiberLocker locker() {
		return KFiberLocker(lock);
	}
private:
	void getVhIndex(KWStream& s, KVirtualHost* vh, int id);
	bool internalAddVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh, KVirtualHost* ov);
	bool internalRemoveVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh);
	void getAllVhHtml(KWStream& s);
	void getVhDetail(KWStream& s, KVirtualHost* vh, bool edit);
	/*
	 * �������������б�
	 */
	std::map<KString, KVirtualHost*> avh;
	/*
	* �󶨵�������
	*/
	void InternalBindVirtualHost(KVirtualHost* vh);
	void InternalUnBindVirtualHost(KVirtualHost* vh);
	/*
	* ����ȫ���������������ϡ�
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
