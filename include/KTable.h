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

////////////////////////////////////////////////

class KChain;
#define TABLE_CONTEXT 	"table"

class KTable : public KJump {
public:
	~KTable();
	KTable();
	kgl_jump_type match(KHttpRequest *rq, KHttpObject *obj, unsigned& checked_table, KJump **jump, KFetchObject **fo);
	std::string addChainForm(KChain *chain,u_short accessType);
	void htmlTable(std::stringstream &s,const char *vh,u_short accessType);
	int insertChain(int index, KChain *newChain);
	bool delChain(std::string name);
	bool editChain(std::string name,KUrlValue *urlValue,KAccess *kaccess);
	bool delChain(int index);
	bool downChain(int index);
	bool editChain(int index, KUrlValue *urlValue,KAccess *kaccess);
	bool addAcl(int index, std::string acl, bool mark,KAccess *kaccess);
	bool delAcl(int index, std::string acl, bool mark);
	bool downModel(int index, std::string acl, bool mark);
	void empty();
	friend class KAccess;
public:
	bool startElement(KXmlContext* context) override {
		return true;
	}
	bool startElement(KXmlContext *context,KAccess *kaccess);
	bool startCharacter(KXmlContext *context, char *character, int len) override;
	bool endElement(KXmlContext *context) override;
	/*

	*/
	void buildXML(std::stringstream &s,int flag) override;
	bool buildXML(const char *chain_name,std::stringstream &s,int flag);

private:
	KChain *findChain(const char *name);
	KChain *findChain(int index);
	void removeChain(KChain *chain);
	void pushChain(KChain *chain);
	void chainChangeName(std::string oname,KChain *chain);
	//ÐÂµÄÁ´
	//std::vector<KChain *> chain;
	KChain *head;
	KChain *end;
	std::map<std::string,KChain *> chain_map;
	KChain *curChain;
	bool ext;
};
#endif /*KTABLE_H_*/
