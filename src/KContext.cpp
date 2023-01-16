#include "kforwin32.h"
//#include "KHttpTransfer.h"
#include "KCache.h"
#include "KContext.h"
#include <assert.h>
#if 0
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
	have_stored = false;
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
#endif
