#ifndef KACSERVERMANAGER_H_
/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#define KACSERVERMANAGER_H_
#include <vector>
#include "KApiRedirect.h"
#include "KSingleAcserver.h"
#include "KMultiAcserver.h"
#include "KCmdPoolableRedirect.h"
#include "KRWLock.h"
#include "KExtendProgram.h"
#include "KConfigTree.h"
#include <string>
class KAcserverManager
{
public:
	KAcserverManager();
	virtual ~KAcserverManager();
	void on_server_event(kconfig::KConfigTree* tree, const khttpd::KXmlNode* xml, kconfig::KConfigEventType ev);
	void on_cmd_event(kconfig::KConfigTree* tree, const khttpd::KXmlNode* xml, kconfig::KConfigEventType ev);
	void on_api_event(kconfig::KConfigTree* tree, const khttpd::KXmlNode* xml, kconfig::KConfigEventType ev);
	void acserverList(KWStream& s, const KString& name = "");
	void apiList(KWStream& s, const KString& name = "");
#ifdef ENABLE_VH_RUN_AS	
	void cmdList(KWStream& s, const KString& name = "");
#ifdef ENABLE_ADPP
	/*
	 * flush the cmd extend process cpu usage.
	 */
	void flushCpuUsage(ULONG64 cpuTime);
#endif
	void shutdown();
	void refreshCmd(time_t nowTime);
	void getProcessInfo(KWStream &s);
	void killCmdProcess(USER_T user);
	void killAllProcess(KVirtualHost* vh);
	/* 全部准备好了，可以加载所有的api了。*/
	void loadAllApi();
#endif
	void unloadAllApi();
	bool remove_server(const KString &name, KString& err_msg);
#ifdef ENABLE_MULTI_SERVER
	void macserverList(KWStream& s, const KString& name="");
	void macserver_node_form(KWStream& s, const KString&, KString action, unsigned nodeIndex);
	bool macserver_node(KXmlAttribute& attribute, KString& errMsg);
	KMultiAcserver* refsMultiAcserver(const KString& name)
	{
		lock.RLock();
		KMultiAcserver* as = getMultiAcserver(name);
		if (as) {
			as->add_ref();
		}
		lock.RUnlock();
		return as;
	}
	bool addMultiAcserver(KMultiAcserver* as)
	{
		lock.WLock();
		auto  it = mservers.find(as->name);
		if (it != mservers.end()) {
			lock.WUnlock();
			return false;
		}
		mservers.insert(std::pair<KString, KMultiAcserver*>(as->name, as));
		lock.WUnlock();
		return true;
	}
#endif
	std::vector<KString> getAcserverNames(bool onlyHttp);
	std::vector<KString> getAllTarget();
	bool new_server(bool overFlag, KXmlAttribute& attr, KString& err_msg);
#ifdef ENABLE_VH_RUN_AS
	bool cmdForm(KXmlAttribute& attribute, KString& errMsg);
	KCmdPoolableRedirect* newCmdRedirect(std::map<KString, KString>& attribute,
		KString& errMsg);
	KCmdPoolableRedirect* refsCmdRedirect(const KString& name);
	bool cmdEnable(const KString&, bool enable);
	bool delCmd(const KString& name, KString& err_msg) {
		return remove_item("cmd", name, err_msg);
	}
	bool addCmd(KCmdPoolableRedirect* as)
	{
		lock.WLock();
		auto it = cmds.find(as->name);
		if (it != cmds.end()) {
			lock.WUnlock();
			return false;
		}
		cmds.insert(std::pair<KString, KCmdPoolableRedirect*>(as->name, as));
		lock.WUnlock();
		return true;
	}
#endif
	bool apiEnable(const KString&, bool enable);
	bool delApi(const KString& name, KString& err_msg) {
		return remove_item("api", name, err_msg);
	}
	bool apiForm(KXmlAttribute& attribute, KString& errMsg);
	KSingleAcserver* refsSingleAcserver(const KString& name);
	KPoolableRedirect* refsAcserver(const KString& name);
	KRedirect* refsRedirect(const KString &target);
	KApiRedirect* refsApiRedirect(const KString& name);
	void clearImportConfig();
	friend class KAccess;
	friend class KHttpManage;
private:
	KSingleAcserver* getSingleAcserver(const KString& table_name);
#ifdef ENABLE_MULTI_SERVER
	KMultiAcserver* getMultiAcserver(const KString&  table_name);
	std::map<KString, KMultiAcserver*> mservers;
#endif
	KPoolableRedirect* getAcserver(const KString& table_name);
	KApiRedirect* getApiRedirect(const KString& name);
#ifdef ENABLE_VH_RUN_AS
	KCmdPoolableRedirect* getCmdRedirect(const KString& name);
	std::map<KString, KCmdPoolableRedirect*> cmds;
#endif
	std::map<KString, KSingleAcserver*> acservers;
	std::map<KString, KApiRedirect*> apis;
	bool remove_item(const KString& scope, const KString& name, KString& err_msg);
	bool new_item(const KString& scope, bool is_update, KXmlAttribute& attr, KString& err_msg);
	KRWLock lock;
	KRLocker get_rlocker() {
		return KRLocker(&lock);
	}
	KWLocker get_wlocker() {
		return KWLocker(&lock);
	}
};
void on_server_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent *ev);
void on_api_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_cmd_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
#endif /*KACSERVERMANAGER_H_*/
