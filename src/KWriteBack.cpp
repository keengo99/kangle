#include "KWriteBack.h"
#include "KHttpObject.h"
#include "KBufferFetchObject.h"
#include "kmalloc.h"
using namespace std;

std::string KWriteBack::getMsg()
{
	std::stringstream s;
	kgl_str_t ret;
	kgl_pool_t* pool = kgl_create_pool(128);
	KHttpKeyValue::get_request_line(pool, status_code, &ret);
	s.write(ret.data, ret.len);
	kgl_destroy_pool(pool);
	KHttpHeader* h = header;
	while (h) {
		kgl_str_t attr;
		kgl_get_header_name(h, &attr);
		s << "\r\n" << attr.data << ": " << (h->buf + h->val_offset);
		h = h->next;
	}
	if (keep_alive) {
		s << "\r\nConnection: keep-alive";
	}
	s << "\r\n\r\n";
	if (body.getSize()) {
		s << body.getString();
	}
	return s.str();
}
void KWriteBack::setMsg(std::string msg)
{
	if (header) {
		free_header_list(header);
		header = NULL;
	}
	body.clean();
	if (msg.empty()) {
		return;
	}
	char* buf = strdup(msg.c_str());
	KWriteBackParser parser;
	char* data = buf;
	char* end = data + msg.size();
	parser.Parse(&data, end);
	status_code = parser.status_code;
	keep_alive = parser.keep_alive;
	header = parser.steal_header();
	size_t len = end - data;
	if (len > 0) {
		body.write_all(data, (int)len);
	}
	xfree(buf);
}
void KWriteBack::buildRequest(KHttpRequest* rq, KFetchObject** fo)
{
	rq->response_status(status_code);
	KHttpHeader* h = header;
	while (h) {
		rq->response_header(h, false);
		h = h->next;
	}	
	if (!keep_alive) {
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
	}
	if (rq->sink->data.meth != METH_HEAD) {
		KAutoBuffer buffer(rq->sink->pool);
		buffer.write_all(body.getBuf(), body.getSize());
		*fo = new KBufferFetchObject(buffer.getHead(),buffer.getLen());
		return;
	}
	*fo = new KBufferFetchObject(NULL, body.getSize());
}
