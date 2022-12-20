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
/*
 * KCgiEnv.h
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 * 为unix的cgi提供env服务的类，重成环境变量
 */

#ifndef KCGIENV_H_
#define KCGIENV_H_
#include <list>
#include "KEnvInterface.h"

class KCgiEnv: public KEnvInterface {
public:
	KCgiEnv();
	virtual ~KCgiEnv();
	bool addEnv(const char *attr, const char *val) override;
	bool add_env(const char* attr, size_t attr_len, const char* val, size_t val_len) override;
	bool addEnv(const char *env);
	bool addEnvEnd() override;
	char **dump_env();
private:
	std::list<char *> m_env;
	char **env;
};

#endif /* KCGIENV_H_ */
