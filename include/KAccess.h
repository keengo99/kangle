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
#ifndef KACCESS_H_
#define KACCESS_H_
#include <list>
#include <vector>
#include "global.h"
#include "KAcl.h"
#include "KMark.h"
#include "KMutex.h"
#include "KHttpRequest.h"
#include "KAcserver.h"
//#include "KFilter.h"
#include "kfiber_sync.h"
#define BEGIN_TABLE	"BEGIN"
#define REQUEST		0
#define RESPONSE	1

#define CHAIN_XML_DETAIL   0
#define XML_EXPORT         1
#define CHAIN_XML_SHORT    2
#define CHAIN_SKIP_EXT    (1<<10)
#define CHAIN_RESET_FLOW  (1<<11)

class KTable;
class KVirtualHostEvent;
class WhmContext;
class KAccess: public KXmlSupport {
public:
	KAccess();
	virtual ~KAccess();
	void destroy();
	int check(KHttpRequest *rq, KHttpObject *obj);
	//POSTMAP 只在RESPONSE里有效，在映射完物理文件后调用
	int checkPostMap(KHttpRequest *rq,KHttpObject *obj);
	static void loadModel();
public:
	//void checkExpireChain();
	void printFilter();
	//	std::string addChainForm(std::string table_name, int index, bool add=true);
	std::string chainList();
	
	static int getType(int type);
	static int32_t ShutdownMarkModule();
	void htmlChainAction(std::stringstream &s, int jump_type, KJump *jump,
			bool showTable, std::string skipTable);
	bool parseChainAction(std::string action, int &jumpType,
			std::string &jumpName);
	static void buildChainAction(int jumpType, KJump *jump, std::stringstream &s);
	void setChainAction(int &jump_type, KJump **jump, std::string name);
	std::string
	addChainForm(const char *vh,std::string table_name, int index, bool add = true);
	int newChain(std::string table_name, int index,KUrlValue *urlValue=NULL);
	bool downChain(std::string table_name, int index);
	bool delChain(std::string table_name, int index);
	bool delChain(std::string table_name, std::string name);
	void changeFirst(int jump_type, std::string name);
	bool addAcl(std::string table_name, int index, std::string acl, bool mark);
	bool delAcl(std::string table_name, int index, std::string acl, bool mark);
	bool downModel(std::string table_name, int index, std::string acl, bool mark);
	bool editChain(std::string table_name, int index, KUrlValue *urlValue);
	bool editChain(std::string table_name, std::string name, KUrlValue *urlValue);
	void clearImportConfig();
	bool isGlobal();
	void copy(KAccess &a);
	void setType(u_short type)
	{
		this->type = type;
		if (type==REQUEST) {
			qName = "request";
		} else {
			qName = "response";
		}
	}
	void setGlobal(bool globalFlag)
	{
		this->globalFlag = globalFlag;
	}
public:
	std::string htmlAccess(const char *vh="");
	bool emptyTable(std::string table_name, std::string &err_msg);
	bool newTable(std::string table_name, std::string &err_msg);
	bool delTable(std::string table_name, std::string &err_msg);
	bool renameTable(std::string name_from, std::string name_to,std::string &err_msg);
	void listTable(KVirtualHostEvent *ctx);
	bool listChain(std::string table_name,const char *chain_name,KVirtualHostEvent *ctx,int flag);
public:
	void startXml(const std::string &encoding) override;
	void endXml(bool result) override;
	bool startElement(KXmlContext *context) override;
	bool startCharacter(KXmlContext *context, char *character, int len) override;
	bool endElement(KXmlContext *context) override;
	void buildXML(std::stringstream &s, int flag) override;
public:
	const char *qName;
	u_short type;
	bool globalFlag;
	bool actionParsed;
	friend class KChain;
	static std::map<std::string,KAcl *> aclFactorys[2];
	static std::map<std::string,KMark *> markFactorys[2];
	static bool addAclModel(u_short type,KAcl *acl,bool replace=false);
	static bool addMarkModel(u_short type,KMark *acl,bool replace=false);
	static void releaseRunTimeModel(KModel *model);
	static int whmCallRunTimeModel(std::string name,WhmContext *ctx);
private:
	void htmlRadioAction(std::stringstream &s, int *jump_value, int jump_type,
		KJump *jump, int my_jump_type, std::string my_type_name,
			std::vector<std::string> table_names);	
private:
	void setChainAction();
	void inter_destroy();
	kfiber_rwlock* rwlock;
	int default_jump_type;
	std::string jump_name;
	KJump *default_jump;	
	KTable *curTable;
	KTable *begin;
	KTable *postMap;
	std::map<std::string,KTable *> tables;
	KTable *getTable(std::string table_name);
	bool InternalNewTable(std::string table_name, std::string& err_msg);
	std::vector<std::string> getTableNames(std::string skipName,bool show_global);
private:
	static KModel *getRunTimeModel(std::string name);
	static void addRunTimeModel(KModel *m);
	static std::map<std::string,KModel *> runtimeModels;
	static kfiber_mutex *runtimeLock;
};
extern KAccess kaccess[2];
#endif /*KACCESS_H_*/
