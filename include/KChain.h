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

class KChain final
{
public:
	KChain();
	~KChain();
	bool match(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo);
	void parse_config(KAccess* access, const khttpd::KXmlNodeBody* xml);
	void get_acl_short_html(KWStream& s);
	void get_mark_short_html(KWStream& s);
	void clear();
	friend class KAccess;
	friend class KTable;
private:
	void getModelHtml(KModel* model, KWStream& s, int type, int index);
private:
	uint32_t hit_count;
	kgl_jump_type jump_type;
	KSafeJump jump;
	std::list<KAcl*> acls;
	std::list<KMark*> marks;
};
#endif /*KCHAIN_H_*/
