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
#ifndef KACL_H_
#define KACL_H_
#include "KXmlSupport.h"
#include "KHttpRequest.h"
#include "KHttpObject.h"
#include "KModel.h"
#define 	ACL_CONTEXT		"acl"

/*
 * ���ʿ���
 */
class KAcl : public KModel {
public:
	KAcl() {
	}
	virtual KAcl* new_instance() = 0;
	virtual bool match(KHttpRequest* rq, KHttpObject* obj) = 0;
protected:
	virtual ~KAcl() {
	}
};
using KSafeAcl = KSharedObj<KAcl>;
#endif
