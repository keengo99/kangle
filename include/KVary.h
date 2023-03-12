#ifndef KVARY_H_99
#define KVARY_H_99
#include <string.h>
#include "global.h"
#include "kmalloc.h"
#include "KHttpLib.h"
#include "ksapi.h"
#include "KStringBuf.h"

#define KGL_VARY_EXTEND_CHAR   '|'


class KUrl;
class KHttpRequest;
class KHttpObject;

class KVary
{
public:
	KVary()
	{
		memset(this, 0, sizeof(*this));
	}
	~KVary()
	{
		if (key) {
			xfree(key);
		}
		if (val) {
			xfree(val);
		}
	}
	KVary *Clone()
	{
		if (key == NULL || val==NULL) {
			return NULL;
		}
		KVary *vary = new KVary;
		vary->key = strdup(key);
		if (val) {
			vary->val = strdup(val);
		}
		return vary;
	}
	char *key;
	char *val;
};
struct KUrlKey
{
	KUrl *url;
	KVary *vary;
};
kgl_auto_cstr get_obj_url_key(KHttpObject *obj, int *len);
void update_url_vary_key(KUrlKey *uk, const char *key);
bool register_vary_extend(kgl_vary *vary);
bool build_vary_extend(KHttpRequest *rq, KWStream*s, const char *name, const char *value);
bool response_vary_extend(KHttpRequest *rq, KWStream*s, const char *name, const char *value);
#endif

