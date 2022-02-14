#ifndef KVIRTUALHOSTDATABASE_H
#define KVIRTUALHOSTDATABASE_H
#include <map>
#include <list>
#include <string>
#include "global.h"
#include "utils.h"
#include "vh_module.h"
#include "KVirtualHost.h"
#include "KDsoModule.h"
#include "kfiber_sync.h"
#define VH_INFO_HOST       0
#define VH_INFO_ERROR_PAGE 1
#define VH_INFO_INDEX      2
#define VH_INFO_MAP        3
#define VH_INFO_ALIAS      4
#define VH_INFO_MIME       5
#define VH_INFO_BIND       7
#define VH_INFO_HOST2      8
#define VH_INFO_ENV        100

class KVirtualHostDatabase
{
public:
	KVirtualHostDatabase();
	~KVirtualHostDatabase();
	bool flushVirtualHost(const char *vhName,bool initEvent,KVirtualHostEvent *ctx);
	bool loadVirtualHost(KVirtualHostManage *vm,std::string &errMsg);

	bool parseAttribute(std::map<std::string,std::string> &attribute);
	//检查数据库连接是否正常
	bool check();
	bool isSuccss()
	{
		return lastStatus;
	}
	bool isLoad();
	//void clear();
	bool ext;

private:
	kgl_vh_connection createConnection();
	void freeConnection(kgl_vh_connection cn);
	bool loadInfo(KVirtualHost *vh, kgl_vh_connection cn);
	KVirtualHost *newVirtualHost(kgl_vh_connection cn, std::map<std::string,std::string> &attribute, KVirtualHostManage *vm, KVirtualHost *ov);
	kfiber_mutex* lock;
	vh_module vhm;
	bool lastStatus;
	KDsoModule vhm_handle;
};
extern KVirtualHostDatabase vhd;
#endif
