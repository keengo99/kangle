#include "kforwin32.h"
//#include "KHttpTransfer.h"
#include "KCache.h"
#include "KContext.h"
#include <assert.h>
void KContext::dead_old_obj()
{
	if (old_obj == NULL) {
		return;
	}
	cache.dead(old_obj, __FILE__, __LINE__);
	old_obj->release();
	old_obj = NULL;
}
void KContext::clean()
{
	if (st) {
		delete st;
	}
	kassert(obj == NULL);
	kassert(old_obj == NULL);
	memset(this,0,sizeof(KContext));
}
void KContext::store_obj(KHttpRequest *rq)
{
	if (haveStored) {
		return ;
	}
	haveStored = true;
	//printf("old_obj = %p\n",old_obj);
	if (old_obj) {
		//send from cache
		assert(obj);
		KBIT_CLR(rq->filter_flags,RQ_SWAP_OLD_OBJ);
		if (obj->data->i.status_code == STATUS_NOT_MODIFIED) {
			//更新obj
			//删除新obj
			assert(old_obj->in_cache);
			cache.rate(old_obj);
			return;
		}
		if (obj->in_cache == 0) {
			if (stored_obj(rq, obj, old_obj)) {
				cache.dead(old_obj, __FILE__, __LINE__);
				return;
			}
		}
		if (KBIT_TEST(rq->filter_flags, RF_ALWAYS_ONLINE) ||
			(KBIT_TEST(old_obj->index.flags, OBJ_IS_GUEST) && !KBIT_TEST(rq->filter_flags, RF_GUEST))
			) {
			//永久在线，并且新网页没有存储
			//或者是会员访问了游客缓存
			//旧网页继续使用
			cache.rate(old_obj);
		} else {
			cache.dead(old_obj,__FILE__,__LINE__);
		}
		return;
	}
	if (obj == NULL) {
		return;
	}
	//check can store
	if (obj->in_cache==0) {
		stored_obj(rq, obj,old_obj);
	} else {
		assert(obj->in_cache==1);
		cache.rate(obj);
	}
}
void KContext::clean_obj(KHttpRequest *rq,bool store_flag)
{
	if (store_flag) {
		store_obj(rq);
	}
	if (old_obj) {
		old_obj->release();
		old_obj = NULL;
	}
	if (obj) {
		obj->release();
		obj = NULL;
	}
	haveStored = false;
}
void KContext::push_obj(KHttpObject *obj)
{
	assert(old_obj==NULL);
	if (this->obj==NULL) {
		this->obj = obj;
		return;
	}
	this->old_obj = this->obj;
	this->obj = obj;
}
void KContext::pop_obj()
{
	if (old_obj) {
		assert(obj);
		obj->release();
		obj = old_obj;
		old_obj = NULL;
	}
}
