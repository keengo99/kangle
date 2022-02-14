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
 *  Author: KangHongjiu <keengo99@gmail.com>  2011-07-18
 */
#ifndef KACCESSPARSER_H_
#define KACCESSPARSER_H_
#include "KXmlEvent.h"
#include "KAccess.h"

class KAccessParser: public KXmlEvent {
public:
	KAccessParser();
	virtual ~KAccessParser();
	bool startElement(KXmlContext *context,
			std::map<std::string, std::string> &attribute);
	bool startCharacter(KXmlContext *context, char *character, int len);
	bool endElement(KXmlContext *context);
	bool parseFile(std::string file, KAccess *access);
	bool parseString(const char *str,KAccess *access);
	void startXml(const std::string &encoding);
	void endXml(bool success);
private:
	KAccess *access;
};
#endif /* KACCESSPARSER_H_ */
