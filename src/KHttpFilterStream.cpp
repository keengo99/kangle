#include "KHttpFilterStream.h"
#include "KHttpFilterHook.h"
#include "KHttpFilterDso.h"
#include "KHttpRequest.h"
#include "KHttpFilterContext.h"
#ifdef ENABLE_KSAPI_FILTER
#if 0
struct KMemoryBlock
{
	char *buf;
	KMemoryBlock *next;
};
class KAutoMemory
{
public:
	KAutoMemory()
	{
		head = NULL;
	}
	~KAutoMemory()
	{
		while (head) {
			KMemoryBlock *n = head->next;
			xfree(head->buf);
			xfree(head);
			head = n;
		}
	}
	char *alloc(int size)
	{
		char *buf = (char *)xmalloc(size);
		if (buf==NULL) {
			return NULL;
		}
		KMemoryBlock *block = (KMemoryBlock *)xmalloc(sizeof(KMemoryBlock));
		block->buf = buf;
		block->next = head;
		head = block;
		return buf;
	}
private:
	KMemoryBlock *head;
};
static void * WINAPI stackAlloc(void *ctx,DWORD size)
{
	KAutoMemory *memory = (KAutoMemory *)ctx;
	return memory->alloc(size);
}
StreamState KHttpFilterStream::write_all(const char *buf, int len)
{
	StreamState result;
	kgl_filter_data data;
	data.buffer = (void *)buf;
	data.reserved = 0;
	data.stack_alloc = stackAlloc;
	for (;;) {
		KAutoMemory memory;
		data.length = len;
		data.stack_ctx = (void *)&memory;
		DWORD disabled_flags = rq->http_filter_ctx->restore(hook->dso->index);
		if (TEST(disabled_flags,notificationType)) {
			//disabled
			return st->write_all(buf,len);
		}
		DWORD ret = hook->dso->kgl_filter_process(
					&rq->http_filter_ctx->ctx,
					notificationType,
					&data);
		rq->http_filter_ctx->save(hook->dso->index);
		switch (ret) {
		case KF_STATUS_REQ_FINISHED:
			SET(rq->flags,RQ_CONNECTION_CLOSE);
		case KF_STATUS_REQ_FINISHED_KEEP_CONN:
			return STREAM_WRITE_FAILED;
		case KF_STATUS_REQ_HANDLED_NOTIFICATION:
			return STREAM_WRITE_SUCCESS;
		case KF_STATUS_REQ_NEXT_NOTIFICATION:
		case KF_STATUS_REQ_READ_NEXT:
			if (data.length>0) {
				result = st->write_all((char *)data.buffer,data.length);
				if (result!=STREAM_WRITE_SUCCESS) {
					return result;
				}
				if (ret==KF_STATUS_REQ_READ_NEXT) {
					len = 0;
					continue;
				}
			}
			return STREAM_WRITE_SUCCESS;
		default:
			return STREAM_WRITE_FAILED;
		}
	}
	return STREAM_WRITE_SUCCESS;
}
StreamState KHttpFilterStream::write_end()
{
	StreamState result = this->write_all(NULL,0);
	if (result!=STREAM_WRITE_SUCCESS) {
		return result;
	}
	return st->write_end();
}
#endif
#endif
