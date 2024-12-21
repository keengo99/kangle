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
#ifndef WHMEXTEND_H_
#define WHMEXTEND_H_
#include "WhmContext.h"
#include "KCountable.h"
#include "whm.h"
class WhmExtend;
typedef int (WhmExtend::*whm_call_ptr) (const char *call_name, const char* event_type, WhmContext* context);

class WhmExtend : public KXmlEvent{
public:
	WhmExtend();
	virtual ~WhmExtend();
	virtual bool init(KString&whmFile);
	virtual whm_call_ptr parse_call(const KString &call_name) {
		return nullptr;
	}
	/* @deprecated use parse_call instead */
	virtual int call(const char* callName, const char* eventType, WhmContext* context) {
		return WHM_CALL_NOT_FOUND;
	}
	KString name;
	virtual const char *getType() = 0;
	virtual void flush()
	{
	}
protected:	
	KString file;
};

#endif /* WHMEXTEND_H_ */
