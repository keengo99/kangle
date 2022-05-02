#ifndef KSQLITEDISKCACHEINDEX_H
#define KSQLITEDISKCACHEINDEX_H
#include "KDiskCacheIndex.h"
#ifdef ENABLE_SQLITE_DISK_INDEX
#include "sqlite3.h"
#include "klog.h"
class KSqliteDiskCacheIndex : public KDiskCacheIndex
{
public:
	KSqliteDiskCacheIndex()
	{
		db = NULL;
		fileName = NULL;
	}
	~KSqliteDiskCacheIndex()
	{
		close();
		if (fileName) {
			xfree(fileName);
		}
	}
	void close()
	{
		if (db) {
			klog(KLOG_ERR, "close dci db...\n");
			sqlite3_close(db);
			db = NULL;
			klog(KLOG_ERR, "closed dci db done.\n");
		}
	}
	bool begin();
	bool commit();
	bool create(const char *indexFileName);
	bool open(const char *indexFileName);
	INT64 memory_used();
protected:
	bool add(unsigned filename1,unsigned filename2,const char *url,const char *data,int dataLen);
	bool del(unsigned filename1,unsigned filename2);
	bool update(unsigned filename1,unsigned filename2,const char *buf,int len);
	
	bool load(loadDiskCacheIndexCallBack callBack);
private:
	bool check();
	void setting();
	sqlite3 *db;
	char *fileName;
};
#endif
#endif
