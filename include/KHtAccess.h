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
#include "utils.h"
#include "KCountable.h"
#include "KAccess.h"
#include "KFileName.h"
#include "KHtModule.h"
#include "do_config.h"

/*
 * 转换apache格式的.htaccess到kangle的KAccess
 */
class KApacheConfig {
public:
	KApacheConfig(bool isHtaccess);
	virtual ~KApacheConfig();
	bool load(KFileName *file,std::stringstream &s);
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
	const char *prefix;
	bool isHtaccess;
	std::map<char *,char *,lessp_icase> attribute;
	std::string context;
private:
	bool load(KFileName *file);
	bool parse(char *buf);
	bool process(const char *cmd,std::vector<char *> &item);
	void getXml(std::stringstream &s);
	std::string serverRoot;
	int includeLevel;
	std::list<KListenHost> listens;
	std::list<KHtModule *> modules;
	unsigned tableid;
};
#endif /* KHTACCESS_H_ */
