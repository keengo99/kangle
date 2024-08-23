#include <string.h>
#include <string>
#include <stdlib.h>
#include <vector>
#include "KHttpObjectHash.h"
#include "kmalloc.h"
#include "KCache.h"
//设置obj的content_length;
void set_obj_size(KHttpObject *obj, INT64 content_length) {
	//如果没有在hash中,则直接设置
	if (obj->list_state == LIST_IN_NONE) {
		obj->index.content_length = content_length;
		return;
	}
	KHttpObjectHash *hash = cache.getHash(obj->h);
	hash->MemObjectSizeChange(obj,content_length);	
	return;
}
