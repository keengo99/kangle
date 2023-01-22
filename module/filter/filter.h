#ifndef DSO_FILTER_H_99
#define DSO_FILTER_H_99
#include "ksapi.h"
#include "kstring.h"

struct kgl_footer {
	kgl_refs_string* data;
	bool head;
	bool replace;
	bool added;
};
class kgl_filter_footer
{
public:
	kgl_filter_footer() {
		memset(this, 0, sizeof(kgl_filter_footer));
	}
	~kgl_filter_footer() {
		kstring_release(footer.data);
	}
	kgl_response_body down;
	kgl_footer footer;
};
void register_footer(KREQUEST r, kgl_access_context *ctx, kgl_filter_footer*model_ctx);
#endif
