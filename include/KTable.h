/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef KTABLE_H_
#define KTABLE_H_
#include "ksocket.h"
#include "KJump.h"
#include <list>
#include "KReg.h"
#include "KAccess.h"


struct KConfigFileKey
{
	KConfigFileKey(uint16_t index, const KString &name) : name(name){
		this->index = index;
	}
	KConfigFileKey(uint16_t index, KString&& name) : name(std::forward<KString>(name)) {
		this->index = index;
	}
	KConfigFileKey(const KConfigFileKey& a) : name(a.name) {
		this->index = a.index;
	}
	~KConfigFileKey() {
	}
	uint16_t index;
	KString name;
};
struct KConfigFileLess
{
	bool operator()(const KConfigFileKey& a, const KConfigFileKey& b) const {
		int result = (int)a.index - (int)b.index;
		if (result<0) {
			return true;
		} else if (result > 0) {
			return false;
		}
		return a.name < b.name;
	}
};
class KChain;
#define TABLE_CONTEXT 	"table"
using KSafeChain = std::unique_ptr<KChain>;
class KTable final: public KJump, public kconfig::KConfigListen {
public:
	~KTable();
	KTable(KAccess *access,const KString& name);
	kgl_jump_type match(KHttpRequest *rq, KHttpObject *obj, unsigned& checked_table, KSafeJump &jump, KSafeSource& fo);
	void htmlTable(KWStream &s,const char *vh,u_short accessType);
	bool parse_config(KAccess *access,const khttpd::KXmlNodeBody* xml);
	kconfig::KConfigEventFlag config_flag() const {
		return kconfig::ev_subdir|kconfig::ev_merge;
	}
	bool on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
	void clear();
	friend class KAccess;
private:
	void on_file_event(std::vector<KSafeChain>& chain, kconfig::KConfigEvent* ev);
	KSafeChain parse_chain(const khttpd::KXmlNodeBody* xml);
	//ÐÂµÄÁ´
	KAccess* access;
	std::map<KConfigFileKey, std::vector<KSafeChain>, KConfigFileLess> chains;
};

#endif /*KTABLE_H_*/
