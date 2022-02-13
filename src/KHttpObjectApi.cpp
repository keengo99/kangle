#include "ksapi.h"
#include "KHttpObject.h"
#include "KAccessDsoSupport.h"
KGL_RESULT obj_get_header(KHTTPOBJECT o, LPSTR  name, LPVOID value, LPDWORD size)
{
	KHttpObject *obj = (KHttpObject *)o;
	if (strcasecmp(name, "Vary") == 0) {
		KGL_RESULT ret = KGL_ENO_DATA;
		if (obj->uk.vary == NULL) {
			return ret;
		}
		KMutex *lock = obj->getLock();
		lock->Lock();
		if (obj->uk.vary && obj->uk.vary->key) {
			ret = add_api_var(value, size, obj->uk.vary->key);
		}
		lock->Unlock();
		return ret;
	}
	return add_header_var(value, size, obj->data->headers, name);
}
KHTTPOBJECT get_old_obj(KREQUEST r)
{
	KHttpRequest *rq = (KHttpRequest *)r;
	return (KHTTPOBJECT)rq->ctx->old_obj;
}
kgl_http_object_function http_object_provider = {
	obj_get_header,
	get_old_obj,
};
