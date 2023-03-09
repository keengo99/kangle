#ifndef KDISKCACHEINDEX_H
#define KDISKCACHEINDEX_H
#include "kasync_worker.h"
#include "KDiskCache.h"
#ifdef ENABLE_DB_DISK_INDEX
class KHttpObject;
enum ci_operator
{
	ci_add,
	ci_del,
	ci_update,
	ci_close,
	ci_flush,
	ci_load
};
struct diskCacheOperatorParam
{
	ci_operator op;
	unsigned filename1;
	unsigned filename2;	
	KHttpObjectDbIndex data;
	kgl_auto_cstr url;
};
typedef void (*loadDiskCacheIndexCallBack) (unsigned filename1, unsigned filename2,const char *url,const char *data,int dataLen);
class KDiskCacheIndex
{
public:
	KDiskCacheIndex();
	virtual ~KDiskCacheIndex();
	void start(ci_operator op,KHttpObject *obj);
	virtual bool create(const char *indexFileName) = 0;
	virtual bool open(const char *indexFileName) = 0;
	virtual INT64 memory_used()
	{
		return 0;
	}
	void work(diskCacheOperatorParam *param);
	bool allWorkedDone()
	{
		return kasync_worker_empty(worker);
	}
	kasync_worker *getWorker()
	{
		return worker;
	}
	virtual void close() = 0;
	int load_count;
protected:	
	virtual bool begin() = 0;
	virtual bool commit() = 0;
	virtual bool add(unsigned filename1,unsigned filename2,const char *url,const char *data,int dataLen) = 0;
	virtual bool del(unsigned filename1,unsigned filename2) = 0;
	virtual bool update(unsigned filename1,unsigned filename2,const char *data,int dataLen) = 0;
	virtual bool load(loadDiskCacheIndexCallBack callBack) = 0;
private:
	kasync_worker *worker;
	int transaction_count;
	bool shutdown_flag;
};
extern KDiskCacheIndex *dci;
#endif
#endif
