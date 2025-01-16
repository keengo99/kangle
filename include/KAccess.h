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
#include "KNamedModel.h"
#include "KUrlValue.h"

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
class KAccess final : public kconfig::KConfigListen
{
public:
	KAccess(bool is_global, uint8_t type);
	void clear();
	KAccess* add_ref() {
		katom_inc((void*)&ref);
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
	KModelPtr<KAcl> new_acl(const KString& name, const khttpd::KXmlNodeBody* xml) {
		auto it = KAccess::acl_factorys[type].find(name);
		if (it == KAccess::acl_factorys[type].end()) {
			return nullptr;
		}
		KModelPtr<KAcl> m((*it).second->new_instance());
		if (m.m) {
			m.m->isGlobal = isGlobal();
		}
		m.parse_config(xml);
		m.m->parse_config(xml);
		return m;
	}
	KModelPtr<KMark> new_mark(const KString& name, const khttpd::KXmlNodeBody* xml) {
		auto it = KAccess::mark_factorys[type].find(name);
		if (it == KAccess::mark_factorys[type].end()) {
			return nullptr;
		}
		KModelPtr<KMark> m((*it).second->new_instance());
		if (m.m) {
			m.m->isGlobal = isGlobal();
		}
		m.parse_config(xml);
		m.m->parse_config(xml);
		return m;
	}
	kgl_ref_str_t get_qname() {
		if (type == 0) {
			return "request"_CS;
		}
		return "response"_CS;
	}
	void parse_config(const KXmlAttribute& attr);
	bool on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) override;
	kgl_jump_type check(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo);
	int get_chain(WhmContext *ctx, const KString& table_name);
	void add_chain_form(KWStream& s, const char* vh, const KString& table_name, const KString& file, uint16_t index, size_t id, bool add);
	//POSTMAP 只在RESPONSE里有效，在映射完物理文件后调用
	kgl_jump_type check_post_map(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo);
	static void loadModel();

public:
	uint8_t get_type() {
		return type;
	}
	void setChainAction(kgl_jump_type& jump_type, KSafeJump& jump, const KString& name);
	bool parseChainAction(const KString& action, kgl_jump_type& jumpType, KString& jumpName);
	static void buildChainAction(kgl_jump_type jumpType, const KSafeJump& jump, KWStream& s);
	bool is_table_used(const KString& table_name);
	bool isGlobal();
public:
	KString htmlAccess(const char* vh = "");
	void listTable(KVirtualHostEvent* ctx);
	int dump_chain(KVirtualHostEvent* ctx,const KString table_name);
	int dump_named_module(KVirtualHostEvent* ctx, bool detail);
	/*
	* type=0 acl
	* type=1 mark
	*/
	int dump_available_named_module(KVirtualHostEvent* ctx, int type,bool only_public);
	int get_named_module(KVirtualHostEvent* ctx, const KString &name, bool is_mark);
	int get_module(KVirtualHostEvent* ctx, const KString& name, bool is_mark);
public:
	friend class KChain;
	friend class KTable;
	static std::map<KString, KAcl*> acl_factorys[2];
	static std::map<KString, KMark*> mark_factorys[2];
	static void remove_all_factorys();
	static bool addAclModel(u_short type, KAcl* acl, bool replace = false);
	static bool addMarkModel(u_short type, KMark* acl, bool replace = false);
	static void build_action_attribute(KXmlAttribute& attribute, const KUrlValue& uv);
	bool named_module_can_remove(const KString& name, int type);
private:
	~KAccess();
	void inter_destroy();
	void htmlChainAction(KWStream& s, kgl_jump_type jump_type, KJump* jump, bool showTable, const KString& skipTable);
	void htmlRadioAction(KWStream& s, kgl_jump_type* jump_value, int jump_type, KJump* jump, int my_jump_type, const KString& my_type_name, std::vector<KString>& table_names);
	KModelPtr<KAcl> get_named_acl(const KString& name);
	KModelPtr<KMark> get_named_mark(const KString& name);
	KFiberReadLocker read_lock() {
		return KFiberReadLocker(rwlock);
	}
	KFiberWriteLocker write_lock() {
		return KFiberWriteLocker(rwlock);
	}
	kfiber_rwlock* rwlock;
	volatile uint32_t ref;
	uint8_t type;
	bool global_flag;
	kgl_jump_type default_jump_type;
	KSafeJump default_jump;
	KSafeTable begin;
	KSafeTable post_map;
	std::map<KString, KSafeTable> tables;
	std::map<KString, KSafeNamedModel> named_acls;
	std::map<KString, KSafeNamedModel> named_marks;
	KSafeTable get_table(const KString& table_name,bool always_new=true);
	std::vector<KString> getTableNames(KString skipName, bool show_global);
};
using KSafeAccess = KSharedObj<KAccess>;
extern KAccess* kaccess[2];
#endif /*KACCESS_H_*/
