#include "KHttp2WriteBuffer.h"
#include "KHttp2.h"
#ifdef ENABLE_HTTP2
kgl_http2_event::~kgl_http2_event()
{
	
}
http2_buff::~http2_buff()
{
	kassert(ctx == NULL);
	if (!skip_data_free) {
		xfree(data);
	}
}
void http2_buff::clean(bool fin)
{	
	if (ctx) {
		kassert(ctx->write_wait != NULL);
		if (ctx->write_wait) {
			kgl_http2_event *e = ctx->write_wait;
			ctx->write_wait = NULL;
			kassert(e->fiber);
			kfiber_wakeup(e->fiber,e,  fin ? -1 : e->len);
			//e->result(ctx->data, e->arg, fin?-1:e->len);
			delete e;
		}
		ctx = NULL;
	}
}
http2_buff * KHttp2WriteBuffer::clean()
{
	http2_buff *remove_list = header;
	left = 0;
	header = NULL;
	reset();
	return remove_list;
}
http2_buff *KHttp2HeaderFrame::create(uint32_t stream_id,bool no_body, size_t max_frame_size)
{
	assert(head);
	http2_buff *buf = new http2_buff;
	buf->data = (char *)xmalloc(sizeof(http2_frame_header));
	buf->used = sizeof(http2_frame_header);
	memset(buf->data, 0, sizeof(http2_frame_header));
	last = buf;
	http2_frame_header *h = (http2_frame_header *)buf->data;
	size_t frame_size = 0;
	int type = KGL_HTTP_V2_HEADERS_FRAME;
	if (no_body) {
		h->flags = KGL_HTTP_V2_END_STREAM_FLAG;
	}
	while (head) {
		assert(head->used <= (int)max_frame_size);
		if (frame_size + head->used > max_frame_size) {
			h->set_length_type(frame_size, type);				
			h->stream_id = htonl(stream_id);
			type = KGL_HTTP_V2_CONTINUATION_FRAME;
			frame_size = 0;
			http2_buff *hb = new http2_buff;
			hb->data = (char *)malloc(sizeof(http2_frame_header));
			hb->used = sizeof(http2_frame_header);
			memset(buf->data, 0, sizeof(http2_frame_header));
			h = (http2_frame_header *)hb->data;
			last->next = hb;
			last = hb;
		}
		frame_size += head->used;
		last->next = head;
		last = head;
		head = head->next;
	}
	h->set_length_type(frame_size, type);
	h->stream_id = htonl(stream_id);
	h->flags |= KGL_HTTP_V2_END_HEADERS_FLAG;
	return buf;
}
#endif
