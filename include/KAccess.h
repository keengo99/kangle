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
#ifndef KACCESS_H_
#define KACCESS_H_
#include <list>
#include <vector>
#include "global.h"
#include "KAcl.h"
#include "KMark.h"
#include "KMutex.h"
#include "KHttpRequest.h"
#include "KAcserver.h"
#include "kfiber_sync.h"
#include "KFiberLocker.h"
#include "KConfigTree.h"
#include "KSharedObj.h"

#define BEGIN_TABLE	"BEGIN"
#define REQUEST		0
#define RESPONSE	1

#define CHAIN_XML_DETAIL   0
#define XML_EXPORT         1
#define CHAIN_XML_SHORT    2
#define CHAIN_SKIP_EXT    (1<<10)
#define CHAIN_RESET_FLOW  (1<<11)

class KTable;
class KVirtualHostEvent;
class WhmContext;
using KSafeTable = KSharedObj<KTable>;
void bind_access_config(kconfig::KConfigTree* tree, KAccess* access);
class KAccess: public kconfig::KConfigListen {
public:
	KAccess(bool is_global, u_short type);	
	void clear();
	KAccess* add_ref() {
		katom_inc((void *)&ref);
		return this;
	}
	void release() {
		if (katom_dec((void*)&ref) == 0) {
			delete this;
		}
	}
	kconfig::KConfigEventFlag config_flag() const override {
		return kconfig::ev_self | kconfig::ev_subdir;
	}
	void parse_config(const KXmlAttribute& attr);
	bool on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) override;
	kgl_jump_type check(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo);
	//POSTMAP 只在RESPONSE里有效，在映射完物理文件后调用
	kgl_jump_type checkPostMap(KHttpRequest *rq,KHttpObject *obj, KFetchObject** fo);
	static void loadModel();
public:
	u_short get_type() {
		return type;
	}
	static int32_t ShutdownMarkModule();
	void setChainAction(kgl_jump_type& jump_type, KJump** jump, const KString& name);
	bool parseChainAction(const KString &action, kgl_jump_type&jumpType,KString &jumpName);
	static void buildChainAction(kgl_jump_type jumpType, KJump *jump, KWStream &s);
	bool isGlobal();
public:
	KString htmlAccess(const char *vh="");
	void listTable(KVirtualHostEvent *ctx);
public:
	friend class KChain;
	friend class KTable;
	static std::map<KString,KAcl *> aclFactorys[2];
	static std::map<KString,KMark *> markFactorys[2];
	static bool addAclModel(u_short type,KAcl *acl,bool replace=false);
	static bool addMarkModel(u_short type,KMark *acl,bool replace=false);
private:
	~KAccess();
	void inter_destroy();
	KFiberReadLocker read_lock() {
		return KFiberReadLocker(rwlock);
	}
	KFiberWriteLocker write_lock() {
		return KFiberWriteLocker(rwlock);
	}
	kfiber_rwlock* rwlock;
	kgl_jump_type default_jump_type;
	KJump *default_jump;	
	KSafeTable begin;
	KSafeTable post_map;
	u_short type;
	bool globalFlag;
	volatile uint32_t ref;
	std::map<KString,KSafeTable> tables;
	KSafeTable getTable(const KString &table_name);
	std::vector<KString> getTableNames(KString skipName,bool show_global);
};
using KSafeAccess = KSharedObj<KAccess>;
extern KAccess *kaccess[2];
#endif /*KACCESS_H_*/
