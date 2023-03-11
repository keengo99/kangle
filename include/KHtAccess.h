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

#ifndef KHTACCESS_H_
#define KHTACCESS_H_
#include <map>
#include <list>
#include <string>
#include <memory>
#include "utils.h"
#include "KCountable.h"
#include "KAccess.h"
#include "KFileName.h"
#include "KHtModule.h"
#include "KConfigTree.h"

/*
 * 转换apache格式的.htaccess到kangle的KAccess
 */
class KApacheConfig {
public:
	KApacheConfig(bool isHtaccess);
	virtual ~KApacheConfig();
#if 0
	bool load(KFileName *file,std::stringstream &s);
#endif
	bool load(const char *file);
	/*
	 * 得到唯一table名
	 */
	std::string getTableName()
	{
		std::stringstream s;
		s << "t" << tableid++;
		return s.str();
	}
	void setPrefix(const char *prefix)
	{
		this->prefix = prefix;
	}
	std::string getFullPath(std::string file);
	khttpd::KSafeXmlNode build();
	const char *prefix;
	bool isHtaccess;
	std::map<char *,char *,lessp_icase> attribute;
	std::string context;
private:
	bool load(KFileName *file);
	bool parse(char *buf);
	bool process(const char *cmd,std::vector<char *> &item);
	void getXml(KStringBuf &s);
	std::string serverRoot;
	int includeLevel;
	std::list<KListenHost> listens;
	std::list<KHtModule *> modules;
	unsigned tableid;
};
class KApacheConfigDriver : public kconfig::KConfigFileSourceDriver
{
public:
	khttpd::KSafeXmlNode load(kconfig::KConfigFile* file) override;
	time_t get_last_modified(kconfig::KConfigFile* file) override {
		return kfile_last_modified(file->get_filename()->data);
	}
	bool enable_save() override {
		return false;
	}
	bool enable_scan() override {
		return false;
	}
};
struct _KApacheHtaccessContext
{
	KAccess *access[2];
	kconfig::KConfigFile *file;
	kconfig::KConfigTree ev;
	_KApacheHtaccessContext(const char* filename) : file{ 0 },access { 0 }, ev(nullptr, _KS("")) {
		for (int i = 0; i < 2; ++i) {
			access[i] = new KAccess(false, i);
		}
		ev.add(_KS("request"), access[REQUEST]);
		ev.add(_KS("response"), access[RESPONSE]);
		KString str(filename);
		file = new kconfig::KConfigFile(&ev, nullptr, str.data(), kconfig::KConfigFileSource::Htaccess);
	}
	~_KApacheHtaccessContext() noexcept {
		if (file) {
			file->clear();
			file->release();
		}
		for (int i = 0; i < 2; ++i) {
			if (access[i]) {
				access[i]->release();
			}
		}
		ev.remove(_KS("request"));
		ev.remove(_KS("response"));
		assert(ev.empty());
	}
};
using KApacheHtaccessContext = std::unique_ptr< _KApacheHtaccessContext>;
#endif /* KHTACCESS_H_ */
