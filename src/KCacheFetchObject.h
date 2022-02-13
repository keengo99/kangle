#ifndef KCACHEFETCHOBJECT_H
#define KCACHEFETCHOBJECT_H
#include "KFetchObject.h"
#include "KBuffer.h"
#include "http.h"
/**
* 缓存物件数据源，仅用于内部请求命中
*/
class KCacheFetchObject : public KFetchObject
{
public:
	KCacheFetchObject(KHttpObject *obj)
	{
		header = NULL;
		this->obj = obj;
		obj->addRef();
	}
	~KCacheFetchObject()
	{
		if (header) {
			free(header);
		}
		if (obj) {
			obj->release();
		}
	}
private:
	kbuf *header;
	int left;
	kbuf *hot_buffer;
	KHttpObject *obj;
};
#endif
