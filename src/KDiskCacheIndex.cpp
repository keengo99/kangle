#include "KDiskCacheIndex.h"
#include "KHttpObject.h"
#include "KCache.h"
#ifdef ENABLE_DB_DISK_INDEX
#define AUTO_FLUSH_COUNT   10000
KDiskCacheIndex *dci = NULL;
void loadDiskCacheIndex(unsigned filename1, unsigned filename2, const char *url,const char *data,int dataLen)
{
	if (dataLen!=sizeof(KHttpObjectDbIndex)) {
		klog(KLOG_ERR,"dataLen = %d is not eq sizeof(KHttpObjectDbIndex)=[%d]\n",dataLen,sizeof(KHttpObjectDbIndex));
		return;
	}
	KHttpObjectDbIndex *db_index = (KHttpObjectDbIndex *)data;
	KHttpObject *obj = new KHttpObject;
	obj->dk.filename1 = filename1;
	obj->dk.filename2 = filename2;
	kgl_memcpy(&obj->index, &db_index->index,sizeof(obj->index));
	cor_result result = create_http_object(obj,url,db_index->url_flag_encoding);
	if (result==cor_success) {
		dci->load_count++;
	}
	obj->release();
	return;
}
kev_result diskCacheIndexCallBack(void *data,int msec)
{
	diskCacheOperatorParam *param = (diskCacheOperatorParam *)data;
	dci->work(param);
	delete param;
	return kev_ok;
}
KDiskCacheIndex::KDiskCacheIndex()
{
	worker = kasync_worker_init(1, 0);
	load_count = 0;
	shutdown_flag = false;
	transaction_count = 0;
}
KDiskCacheIndex::~KDiskCacheIndex()
{
	kasync_worker_release(worker);
}
void KDiskCacheIndex::work(diskCacheOperatorParam *param)
{
	if (transaction_count > AUTO_FLUSH_COUNT) {
		transaction_count = 0;
		this->commit();
		this->begin();
	}
	bool result = false;
	switch (param->op) {
	case ci_add:
	{
		transaction_count++;
		assert(param->url);
		result = add(param->filename1,param->filename2,param->url.get(),(char *)&param->data,sizeof(param->data));
		break;
	}
	case ci_update:
	{
		transaction_count++;
		result = update(param->filename1,param->filename2,(char *)&param->data,sizeof(param->data));
		break;
	}
	case ci_del:
		transaction_count++;
		result = del(param->filename1,param->filename2);
		break;
	case ci_close:
		this->commit();
		this->close();
		result = true;
		break;
	case ci_flush:
		if (transaction_count > 0) {
			transaction_count = 0;
			this->commit();
			this->begin();
		}
		break;
	case ci_load:
		result = this->load(loadDiskCacheIndex);
		cache.shutdown_disk(false);
		this->begin();
		break;
	default:
		break;
	}
}
void KDiskCacheIndex::start(ci_operator op,KHttpObject *obj)
{
	diskCacheOperatorParam *param = new diskCacheOperatorParam;
	memset(param,0,sizeof(diskCacheOperatorParam));
	param->op = op;
	switch (op) {
	case ci_add:
		param->url = get_obj_url_key(obj,NULL);
	case ci_update:
		param->filename1 = obj->dk.filename1;
		param->filename2 = obj->dk.filename2;
		param->data.url_flag_encoding = obj->uk.url->flag_encoding;
		kgl_memcpy(&param->data.index,&obj->index,sizeof(param->data.index));
		break;
	case ci_del:
		param->filename1 = obj->dk.filename1;
		param->filename2 = obj->dk.filename2;
		break;
	case ci_close:
	case ci_load:
	case ci_flush:
		break;
	default:
		delete param;
		return;
	}
	kasync_worker_start(worker, param, diskCacheIndexCallBack);
}
#endif
