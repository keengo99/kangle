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
#ifndef KCHAIN_H_
#define KCHAIN_H_
#include "KAcl.h"
#include "KMark.h"
#include "KXmlSupport.h"
#include "KCountable.h"
#include "KHttpRequest.h"
#include "KSharedObj.h"
#include "lang.h"
#include <list>
#include "kmalloc.h"
#define CHAIN_CONTEXT	"chain"
class KAccess;
using KSafeAcl = KSharedObj<KAcl>;
using KSafeMark = KSharedObj<KMark>;

class KChain {
public:
	KChain();
	virtual ~KChain();
	bool match(KHttpRequest *rq, KHttpObject *obj, KFetchObject **fo);
	uint32_t hit_count;
	KSafeAcl new_acl(const std::string &name,KAccess *kaccess);
	KSafeMark new_mark(const std::string &name, KAccess* kaccess);	
	void parse_config(KAccess *access, const khttpd::KXmlNodeBody* xml);
	void getAclShortHtml(std::stringstream &s);
	void getMarkShortHtml(std::stringstream &s);
	void clear();
	friend class KAccess;
	friend class KTable;
private:
	void getModelHtml(KModel *model, std::stringstream &s, int type, int index);
private:
	kgl_jump_type jumpType;
	KJump *jump;
	std::list<KSafeAcl> acls;
	std::list<KSafeMark> marks;
};

#endif /*KCHAIN_H_*/
