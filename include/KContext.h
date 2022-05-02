#ifndef KCONTEXT_H
#define KCONTEXT_H
#include "KBuffer.h"
#include "global.h"
#include <assert.h>
#ifndef _WIN32
#include <sys/uio.h>
#endif
class KHttpTransfer;
class KHttpObject;
class KRequestQueue;
class KHttpRequest;
class KWriteStream;
enum modified_type
{
	modified_if_modified,
	modified_if_range_date,
	modified_if_unmodified,
	modified_if_none_match,
	modified_if_range_etag
};
class KContext
{
public:
	inline KContext()
	{
		memset(this,0,sizeof(KContext));
	}
	~KContext()
	{
		assert(obj==NULL && old_obj==NULL);
	}
	void pushObj(KHttpObject *obj);
	void popObj();
	KWriteStream *st;
	KHttpObject *obj;
	KHttpObject *old_obj;
	uint32_t internal:1;
	uint32_t replace : 1;
	uint32_t simulate : 1;
	uint32_t skip_access : 1;
	uint32_t cache_hit_part : 1;
	uint32_t haveStored : 1;
	uint32_t new_object : 1;
	uint32_t know_length : 1;
	uint32_t upstream_connection_keep_alive : 1;
	//connect代理
	uint32_t connection_connect_proxy : 1;
	uint32_t always_on_model : 1;
	//uint32_t upstream_chunked : 1;
	uint32_t response_checked : 1;
	uint32_t fo_need_check : 1;
	uint32_t no_body : 1;
	uint32_t upstream_sign : 1;
	uint32_t parent_signed : 1;
	//client主动关闭
	uint32_t read_huped : 1;
	uint32_t upstream_expected_done : 1;
	uint32_t queue_handled : 1;
	//lastModified类型
	modified_type mt;
	INT64 content_range_length;
	INT64 left_read;	//final fetchobj 使用
	void DeadOldObject();
	void clean();
	void clean_obj(KHttpRequest *rq,bool store_flag = true);
	void store_obj(KHttpRequest *rq);
};

#endif
