#include "KDefaultFetchObject.h"
#include "KStaticFetchObject.h"
#include "KHttpRequest.h"
#include "KDirectoryFetchObject.h"
#include "HttpFiber.h"
#include "KSubVirtualHost.h"
#include "KVirtualHost.h"

KGL_RESULT KDefaultFetchObject::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out)
{
	switch (rq->sink->data.meth) {
	case METH_OPTIONS:
		out->f->write_status(out->ctx, STATUS_OK);
		out->f->write_unknow_header(out->ctx,_KS("Allow"),_KS("GET, HEAD"));
		return out->f->write_header_finish(out->ctx,0, nullptr);
	case METH_GET:
	case METH_HEAD:
		break;
	default:
		out->f->write_status(out->ctx, STATUS_METH_NOT_ALLOWED);
		out->f->write_unknow_header(out->ctx, _KS("Allow"), _KS("GET, HEAD"));
		return out->f->write_header_finish(out->ctx, 0, nullptr);
	}
	if (rq->file->isDirectory()) {
		if (!rq->file->isPrevDirectory()) {
			//url后面不是以/结尾,重定向处理
			KPrevDirectoryFetchObject fo;
			return fo.Open(rq, in, out);
		}
		auto svh = kangle::get_virtual_host(rq);
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
