#include "KSqliteDiskCacheIndex.h"
#include "KHttpLib.h"
#include "KCache.h"
#ifdef ENABLE_SQLITE_DISK_INDEX
#ifndef _WIN32
#include <unistd.h>
#else
#pragma comment(lib,"sqlite3.lib")
#endif
#define CREATE_SQL "CREATE TABLE [cache] ([f1] INTEGER  NULL,[f2] INTEGER  NULL,[url] VARCHAR(1024)  NULL,[data] BLOB  NULL,PRIMARY KEY ([f1],[f2]))"
//#define CREATE_INDEX_SQL "CREATE INDEX [IDX_CACHE_T] ON [cache] ([t]  DESC )"
static int sqliteBusyHandle(void *db,int count)
{
	if (count>20) {
		return 0;
	}
	my_msleep(100);
	return 1;
}
bool KSqliteDiskCacheIndex::create(const char *fileName)
{
	cache.shutdown_disk(false);
	if (this->fileName==NULL) {
		this->fileName = strdup(fileName);
	}
	assert(db==NULL);
	int ret = sqlite3_open(fileName,&db);
	if (ret != SQLITE_OK || db==NULL) {
		return false;
	}
	sqlite3_stmt *stmt;
	ret = sqlite3_prepare(db,CREATE_SQL,-1,&stmt,NULL);
	if (SQLITE_OK != ret) {
		return false;
	}
	ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (ret!=SQLITE_DONE) {
		return false;
	}
	setting();
	return true;
}
bool KSqliteDiskCacheIndex::load(loadDiskCacheIndexCallBack callBack)
{
	if (!this->check()) {
		assert(fileName);
		klog(KLOG_ERR,"recreate the disk cache index database\n");
		this->close();
		unlink(fileName);
		create(fileName);
		rescan_disk_cache();
		return true;
	}
	const char *sql = "select f1,f2,url,data from cache";
	sqlite3_stmt *stmt;
	if (SQLITE_OK != sqlite3_prepare(db,sql,-1,&stmt,NULL)) {
		return false;
	}
	while (SQLITE_ROW == sqlite3_step(stmt)) {
		unsigned f1 = (unsigned)sqlite3_column_int(stmt, 0);
		unsigned f2 = (unsigned)sqlite3_column_int(stmt, 1);
		const char *url = (const char *)sqlite3_column_text(stmt,2);
		int dataLen = sqlite3_column_bytes(stmt,3);
		const char *data = (const char *)sqlite3_column_blob(stmt,3);
		callBack(f1,f2,url,data,dataLen);
	}
	sqlite3_finalize(stmt);
	return true;
}
INT64 KSqliteDiskCacheIndex::memory_used()
{
	return sqlite3_memory_used();

}
bool KSqliteDiskCacheIndex::check()
{
	if (db==NULL) {
		return false;
	}
	const char *sql = "PRAGMA integrity_check(1)";
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare(db,sql,-1,&stmt,NULL);
	if (SQLITE_OK != ret) {
		return false;
	}
	bool result = false;
	if (SQLITE_ROW == sqlite3_step(stmt)) {
		const char *msg = (const char *)sqlite3_column_text(stmt,0);
		if (msg && strcasecmp(msg,"ok")==0) {
			result = true;
		} else {
			klog(KLOG_ERR,"disk cache index database error [%s]\n",msg);
		}
	}
	sqlite3_finalize(stmt);
	return result;
}
bool KSqliteDiskCacheIndex::open(const char *fileName)
{	
	int ret;
	if (this->fileName==NULL) {
		this->fileName = strdup(fileName);
	}
	if (db==NULL) {
		ret = sqlite3_open(fileName,&db);
		if (ret != SQLITE_OK || db==NULL) {
			return false;
		}
	}		
	setting();
	//return check();
	return true;
}
bool KSqliteDiskCacheIndex::add(unsigned filename1,unsigned filename2,const char *url,const char *data,int dataLen)
{
	if (db==NULL) {
		return false;
	}
	const char *sql = "insert into cache (f1,f2,url,data) values (?,?,?,?)";
	sqlite3_stmt *stmt;
	if (SQLITE_OK != sqlite3_prepare(db,sql,-1,&stmt,NULL)) {
		return false;
	}
	sqlite3_bind_int(stmt,1,filename1);
	sqlite3_bind_int(stmt,2,filename2);
	sqlite3_bind_text(stmt,3,url,-1,NULL);
	sqlite3_bind_blob(stmt,4,data,dataLen,NULL);
	int ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return ret==SQLITE_DONE;
}
bool KSqliteDiskCacheIndex::del(unsigned filename1,unsigned filename2)
{
	if (db==NULL) {
		return false;
	}
	const char *sql = "delete from cache where f1=? and f2=?";
	sqlite3_stmt *stmt;
	if (SQLITE_OK != sqlite3_prepare(db,sql,-1,&stmt,NULL)) {
		return false;
	}
	sqlite3_bind_int(stmt,1,filename1);
	sqlite3_bind_int(stmt,2,filename2);
	int ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return ret==SQLITE_DONE;
}
bool KSqliteDiskCacheIndex::update(unsigned filename1,unsigned filename2,const char *data,int dataLen)
{
	if (db==NULL) {
		return false;
	}
	const char *sql = "update cache set data=? where f1=? and f2=?";
	sqlite3_stmt *stmt;
	if (SQLITE_OK != sqlite3_prepare(db,sql,-1,&stmt,NULL)) {
		return false;
	}
	sqlite3_bind_blob(stmt,1,data,dataLen,NULL);
	sqlite3_bind_int(stmt,2,filename1);
	sqlite3_bind_int(stmt,3,filename2);	
	int ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return ret==SQLITE_DONE;
}
bool KSqliteDiskCacheIndex::begin()
{
	if (db==NULL) {
		return false;
	}
	char *errMsg = NULL;
	sqlite3_exec(db,"BEGIN",NULL,NULL,&errMsg);
	if (errMsg) {
		sqlite3_free(errMsg);
	}
	return true;
}
bool KSqliteDiskCacheIndex::commit()
{
	if (db==NULL) {
		return false;
	}
	char *errMsg = NULL;
	sqlite3_exec(db,"COMMIT",NULL,NULL,&errMsg);
	if (errMsg) {
		sqlite3_free(errMsg);
	}
	return true;
}
void KSqliteDiskCacheIndex::setting()
{
	if (db==NULL) {
		return;
	}
	sqlite3_busy_handler(db,sqliteBusyHandle,NULL);
	/*
	char *errMsg = NULL;
	int ret = sqlite3_exec(db,"PRAGMA synchronous = OFF ",NULL,NULL,&errMsg);
	if (errMsg) {
		sqlite3_free(errMsg);
	}
	ret = sqlite3_exec(db,"PRAGMA cache_size = -128",NULL,NULL,&errMsg);
	if (errMsg) {
		sqlite3_free(errMsg);
	}
	*/
	return;
}
#endif

