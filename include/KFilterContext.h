#ifndef KFILTERCONTEXT_H
#define KFILTERCONTEXT_H
#include "KFilterHelper.h"
#include "KHttpObject.h"
struct kgl_out_filter_body {
	kgl_out_filter* filter;
	kgl_response_body_ctx* ctx;
	kgl_list queue;
};
class KOutputFilterContext
{
public:
	KOutputFilterContext()
	{
		klist_init(&filter);
	}
	~KOutputFilterContext()
	{
		kgl_list* pos;
		while (!klist_empty(&filter)) {
			pos = klist_head(&filter);
			kgl_out_filter_body* st = kgl_list_data(pos, kgl_out_filter_body, queue);
			st->filter->tee_body(st->ctx, NULL, NULL);
			klist_remove(pos);
			delete st;
		}
	}
	bool tee_body(KHttpRequest* rq, kgl_response_body* body, int32_t match_flag) {
		kgl_list* pos;
		bool will_change_body_size = false;
		while (!klist_empty(&filter)) {
			pos = klist_head(&filter);
			kgl_out_filter_body* st = kgl_list_data(pos, kgl_out_filter_body, queue);
			if (!KBIT_TEST(st->filter->flags, KGL_FILTER_NOT_CHANGE_LENGTH)) {
				will_change_body_size = true;
			}
			if (KBIT_TEST(st->filter->flags, match_flag)== match_flag) {
				st->filter->tee_body(st->ctx, rq, body);
			} else {
				st->filter->tee_body(st->ctx, NULL, NULL);
			}
			klist_remove(pos);
			delete st;
		}
		return will_change_body_size;
	}
	void add(kgl_out_filter_body*st)
	{
		assert(st->filter->size == sizeof(kgl_out_filter));
		klist_append(&filter, &st->queue);
	}
	kgl_list filter;
};
#endif
