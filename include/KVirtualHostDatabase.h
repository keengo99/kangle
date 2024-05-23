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
#include "KXmlAttribute.h"
#include "KConfigTree.h"

#define VH_INFO_HOST       0
#define VH_INFO_ERROR_PAGE 1
#define VH_INFO_INDEX      2
#define VH_INFO_MAP        3
#define VH_INFO_ALIAS      4
#define VH_INFO_MIME       5
#define VH_INFO_BIND       7
#define VH_INFO_HOST2      8
#define VH_INFO_ENV        100

class KVirtualHostDatabase final: public kconfig::KConfigFileSourceDriver
{
public:
	KVirtualHostDatabase();
	~KVirtualHostDatabase();
	bool parse_config(khttpd::KXmlNodeBody *xml);
	//检查数据库连接是否正常
	bool check();
	bool isSuccss()
	{
		return lastStatus;
	}
	bool isLoad();
	void scan(kconfig::KConfigFileScanInfo* info) override;
	khttpd::KSafeXmlNode load(kconfig::KConfigFile* file) override;
	KFileModified get_last_modified(kconfig::KConfigFile* file) override;
	bool enable_save() override  {
		return false;
	}
	bool enable_scan() override {
		return true;
	}
private:
	KFiberLocker get_locker() {
		return KFiberLocker(lock);
	}
	kgl_vh_connection createConnection();
	void freeConnection(kgl_vh_connection cn);
	bool loadInfo(khttpd::KXmlNodeBody *vh, kgl_vh_connection cn);
	kfiber_mutex* lock;
	vh_module vhm;
	bool lastStatus;
	KDsoModule vhm_handle;
};
extern KVirtualHostDatabase vhd;
#endif
