#include <map>
#include "KVary.h"
#include "KStringBuf.h"
#include "KUrl.h"
#include "utils.h"
#include "KAccessDsoSupport.h"
#include "KHttpObject.h"

static std::map<const char *, kgl_vary *,lessp_icase> varys;

struct kgl_vary_request {
	KHttpRequest *rq;
	KStringBuf *s;
};
char *get_obj_url_key(KHttpObject *obj, int *len)
{
	KStringBuf s;
	obj->uk.url->GetUrl(s);
	if (unlikely(obj->uk.vary!=NULL)) {
		KMutex *lock = obj->getLock();
		lock->Lock();
		if (obj->uk.vary && obj->uk.vary->key && obj->uk.vary->val) {
			s.WSTR("\n");
			s << obj->uk.vary->key;
			s.WSTR("\n");
			s << obj->uk.vary->val;
		}
		lock->Unlock();
	}
	if (len) {
		*len = s.getSize();
	}
	return s.stealString();
}
void update_url_vary_key(KUrlKey *uk, const char *key)
{
	if (key == NULL) {
		kassert(uk->vary);
		xfree(uk->vary->key);
		uk->vary->key = NULL;
		return;
	}
	if (uk->vary == NULL) {
		uk->vary = new KVary;
	}
	if (uk->vary->key) {
		xfree(uk->vary->key);
	}
	uk->vary->key = strdup(key);
	return;
}
bool register_vary_extend(kgl_vary *vary)
{
	std::map<const char *, kgl_vary *, lessp_icase>::iterator it;
	it = varys.find(vary->name);
	if (it != varys.end()) {
		return false;
	}
	varys.insert(std::pair<const char *, kgl_vary *>(vary->name, vary));
	return true;
}
static void vary_response(KCONN cn, const char *str, int len)
{
	kgl_vary_request *vary_rq = (kgl_vary_request *)cn;
	if (vary_rq->s->getSize() > 0) {
		vary_rq->s->write_all(kgl_expand_string(", "));
	}
	vary_rq->s->write_all(str, len);
}
static void vary_write(KCONN cn, const char *str, int len)
{
	kgl_vary_request *vary_rq = (kgl_vary_request *)cn;
	vary_rq->s->write_all(str, len);
}
static KGL_RESULT vary_get_variable(KCONN cn, KGL_VAR type, LPSTR  name, LPVOID value, LPDWORD size)
{
	kgl_vary_request *vary_rq = (kgl_vary_request *)cn;
	return get_request_variable(vary_rq->rq, type, name, value, size);
}
bool response_vary_extend(KHttpRequest *rq, KStringBuf *s, const char *name, const char *value)
{
	std::map<const char *, kgl_vary *, lessp_icase>::iterator it;
	it = varys.find(name);
	if (it == varys.end()) {
		return false;
	}
	if ((*it).second->response == NULL) {
		return false;
	}
	kgl_vary_request vary_rq;
	vary_rq.rq = rq;
	vary_rq.s = s;
	kgl_vary_context ctx;
	memset(&ctx, 0, sizeof(kgl_vary_context));
	ctx.cn = &vary_rq;
	ctx.write = vary_response;
	ctx.get_variable = vary_get_variable;
	return (*it).second->response(&ctx, value);
}
bool build_vary_extend(KHttpRequest *rq, KStringBuf *s, const char *name, const char *value)
{
	std::map<const char *, kgl_vary *, lessp_icase>::iterator it;
	it = varys.find(name);
	if (it == varys.end()) {
		return false;
	}
	kgl_vary_request vary_rq;
	vary_rq.rq = rq;
	vary_rq.s = s;
	kgl_vary_context ctx;
	memset(&ctx, 0, sizeof(kgl_vary_context));
	ctx.cn = &vary_rq;
	ctx.write = vary_write;
	ctx.get_variable = vary_get_variable;
	(*it).second->build(&ctx, value);
	return true;
}
