#include "kforwin32.h"
//#include "KHttpTransfer.h"
#include "KCache.h"
#include "KContext.h"
#include <assert.h>
void KContext::DeadOldObject()
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
	clean_if_none_match();
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
		CLR(rq->filter_flags,RQ_SWAP_OLD_OBJ);
		if (obj->data->status_code == STATUS_NOT_MODIFIED) {
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
		if (TEST(rq->filter_flags, RF_ALWAYS_ONLINE) ||
			(TEST(old_obj->index.flags, OBJ_IS_GUEST) && !TEST(rq->filter_flags, RF_GUEST))
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
void KContext::pushObj(KHttpObject *obj)
{
	assert(old_obj==NULL);
	if (this->obj==NULL) {
		this->obj = obj;
		return;
	}
	this->old_obj = this->obj;
	this->obj = obj;
}
void KContext::popObj()
{
	if (old_obj) {
		assert(obj);
		obj->release();
		obj = old_obj;
		old_obj = NULL;
	}
}
//{{ent
#ifdef ENABLE_DELTA_DECODE
void KContext::getDeltaHeader(KHttpRequest *rq,KStringBuf &sd)
{
	if (TEST(rq->flags, RQ_HAS_GZIP)) {
		sd.write_all(kgl_expand_string("gzip, "));
	}
	if (rq->bo_ctx==NULL && !TEST(rq->flags,RQ_HAVE_RANGE)) {
		//big object skip sd encode
		sd.write_all(kgl_expand_string("sd="));
		if (old_obj && TEST(old_obj->index.flags,OBJ_IS_DELTA)) {
			sd.addHex(old_obj->index.filename1);
			sd.write_all("_",1);
			sd.addHex(old_obj->index.filename2);
		}
	}
}
#endif//}}
