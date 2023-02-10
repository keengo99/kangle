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
#include "lang.h"
#include <list>
#include "kmalloc.h"
#define CHAIN_CONTEXT	"chain"
class KAccess;
class KChain {
public:
	KChain();
	virtual ~KChain();
	bool match(KHttpRequest *rq, KHttpObject *obj, int &jumpType,
			KJump **jumpTable);
	uint32_t hit_count;
	KAcl *addAcl(std::string acl,std::string name,KAccess *kaccess);
	bool delAcl(std::string acl);
	bool downAcl(std::string id);
	bool downMark(std::string id);
	KMark *addMark(std::string mark,std::string name,KAccess *kaccess);
	bool delMark(std::string mark);

	void getAclShortHtml(std::stringstream &s);
	void getMarkShortHtml(std::stringstream &s);
	void getEditHtml(std::stringstream &s,u_short accessType);
	//void getAclHtml(std::stringstream &s);
	bool findAcl(std::string aclName);
	bool findMark(std::string markName);
	/*
	editFlag=false,则表示新加
	*/
	bool edit(KUrlValue *urlValue,KAccess *kaccess,bool editFlag);
	void clear();
	friend class KAccess;
	friend class KTable;
	bool ext;
	//	static void getModelHtml(std::string name,std::string &s,int type,int index);
private:
	bool editModel(KModel *model,std::map<std::string, std::string> &attribute);
	bool editModel(KModel *model, KUrlValue *urlValue);
	void getModelHtml(KModel *model, std::stringstream &s, int type, int index);
public:
	bool startElement(KXmlContext *context,KAccess *kaccess);
	bool startCharacter(KXmlContext *context, char *character, int len);
	bool endElement(KXmlContext *context);
	void buildXML(std::stringstream &s,int flag);
	KAcl *newAcl(std::string acl,KAccess *kaccess);
	KMark *newMark(std::string mark,KAccess *kaccess);
private:
	KJump *jump;
	int jumpType;
	std::string jumpName;
	std::list<KAcl *> acls;
	std::list<KMark *> marks;
	std::string name;
	KAcl *curacl;
	KMark *curmark;
	KChain *next;
	KChain *prev;
};

#endif /*KCHAIN_H_*/
