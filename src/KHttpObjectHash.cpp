#include <string.h>
#include <string>
#include <stdlib.h>
#include <vector>
#include "KHttpObjectHash.h"
#include "kmalloc.h"
#include "KCache.h"
//����obj��content_length;
void set_obj_size(KHttpObject *obj, INT64 content_length) {
	//���û����hash��,��ֱ������
	if (obj->list_state == LIST_IN_NONE) {
		obj->index.content_length = content_length;
		return;
	}
	KHttpObjectHash *hash = cache.getHash(obj->h);
	hash->MemObjectSizeChange(obj,content_length);	
	return;
}
