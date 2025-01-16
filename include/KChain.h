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
#include <vector>
#include "kmalloc.h"
#include "KUrlValue.h"
#include "KModelPtr.h"
#define CHAIN_CONTEXT	"chain"
class KAccess;

class KChain final
{
public:
	KChain();
	~KChain();
	uint32_t match(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) {
		uint32_t result = KF_STATUS_REQ_TRUE;
		bool last_or = false;
		//OR NEXT
		for (auto it = acls.begin(); it != acls.end(); ++it) {
			if (result && last_or) {
				last_or = (*it).is_or;
				continue;
			}
			result = ((*it).m->match(rq, obj) != (*it).revers) ? KF_STATUS_REQ_TRUE : KF_STATUS_REQ_FALSE;
			last_or = (*it).is_or;
			if (!result && !last_or) {
				break;
			}
		}
		if (!result) {
			return KF_STATUS_REQ_FALSE;
		}
		last_or = false;
		for (auto it = marks.begin(); it != marks.end(); ++it) {
			if (result && last_or) {
				last_or = (*it).is_or;
				continue;
			}
			result = (*it).m->process(rq, obj, fo);
			if (KBIT_TEST(result, KF_STATUS_REQ_FINISHED)) {
				++hit_count;
				return result;
			}
			result = (!!result != (*it).revers) ? KF_STATUS_REQ_TRUE : KF_STATUS_REQ_FALSE;
			if (!result && !last_or) {
				break;
			}
			if (fo) {
				++hit_count;
				return KF_STATUS_REQ_TRUE;
			}
		}
		if (result) {
			++hit_count;
		}
		return result;
	}
	void parse_config(KAccess* access, const khttpd::KXmlNodeBody* xml);
	void get_acl_short_html(KWStream& s);
	void get_mark_short_html(KWStream& s);
	void dump(kgl::serializable* s,bool is_short);
	static khttpd::KSafeXmlNode to_xml(KUrlValue& uv);

	void clear();
	friend class KAccess;
	friend class KTable;
private:
	void get_edit_html(kgl::serializable* s);
	void get_edit_html(KWStream& s, u_short accessType);
	template<typename T>
	void getModelHtml(KModelPtr<T> &ptr, KWStream& s, int type, int index) {
		s << "<tr><td><input type=hidden name='begin_sub_form' value='"
			<< (type == 0 ? "acl_"_CS : "mark_"_CS)
			<< ptr.m->getName() << "'>";
		s << "[<a href=\"javascript:delmodel('" << index << "'," << type << ");\">del</a>]";
		if (ptr.named.empty()) {
			s << "<input type = checkbox name = 'or' value = '1' ";
			if (ptr.is_or) {
				s << "checked";
			}
			s << ">OR";
			s << "<input type=checkbox name='revers' value='1' ";
			if (ptr.revers) {
				s << "checked";
			}
			s << ">NOT ";
			s << ptr.m->getName();
		} else {
			s << "named:" << ptr.named;
		}
		s << "</td><td>";
		if (ptr.named.empty()) {
			ptr.m->get_html(s);
		} else {
			s << "<input type='hidden' name='ref' value='" << ptr.named << "'/>";
			s << "named model do not support";
		}
		s << "<input type=hidden name='end_sub_form' value='1'></td></tr>\n";
	}
private:
	uint32_t hit_count;
	kgl_jump_type jump_type;
	KSafeJump jump;
	std::vector<KModelPtr<KAcl>> acls;
	std::vector<KModelPtr<KMark>> marks;
};
#endif /*KCHAIN_H_*/
