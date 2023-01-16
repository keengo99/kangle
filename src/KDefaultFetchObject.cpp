#include "KDefaultFetchObject.h"
#include "KStaticFetchObject.h"
#include "KHttpRequest.h"
#include "KDirectoryFetchObject.h"
#include "HttpFiber.h"
#include "KSubVirtualHost.h"
#include "KVirtualHost.h"

KGL_RESULT KDefaultFetchObject::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out)
{
	if (rq->file->isDirectory()) {
		if (!rq->file->isPrevDirectory()) {
			//url后面不是以/结尾,重定向处理
			if (rq->sink->data.meth == METH_GET) {
				KPrevDirectoryFetchObject fo;
				return fo.Open(rq, in, out);
			} else {
				return out->f->error(out->ctx, STATUS_METH_NOT_ALLOWED,_KS("method not allowed"));
			}
		}
		auto svh = rq->get_virtual_host();
		if (!svh->vh->browse) {
			//如果允许浏览
			return out->f->error(out->ctx, STATUS_FORBIDEN,_KS("You don't have permission to browse."));
		}
		KDirectoryFetchObject fo;
		return fo.Open(rq, in, out);
	}
	KStaticFetchObject fo;
	return fo.Open(rq, in, out);
}
