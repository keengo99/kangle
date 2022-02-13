#include "KDechunkContext.h"
#include "KHttpSink.h"
#include "kfiber.h"
#if 0
kev_result result_dechunk_context(KOPAQUE data, void *arg, int got)
{
	KHttpSink *sink = (KHttpSink *)arg;
	KDechunkContext *dechunk = sink->GetDechunkContext();
	return dechunk->ReadResult(sink, got);
}
int buffer_read_http_sink2(KOPAQUE data, void *arg, LPWSABUF buf, int bufCount)
{
	KHttpSink *sink = (KHttpSink *)arg;
	int bc = ks_get_write_buffers(&sink->buffer, buf, bufCount);
	//printf("buf_len=[%d]\n", buf[0].len);
	return bc;
}
kev_result KDechunkContext::ReadResult(KHttpSink *sink, int got)
{
	//printf("ReadResult got=[%d]\n", got);
	if (got <= 0) {
		return result(sink->GetOpaque(), arg, -1);
	}
	ks_write_success(&sink->buffer, got);
	hot_len += got;
	return ParseBuffer(sink);
}
#endif
bool KDechunkContext::ReadDataFromNet(KHttpSink* sink)
{
	int len;
	char* hot_buf = ks_get_read_buffer(&sink->buffer, &len);
	int got = kfiber_net_read(sink->cn, hot_buf, len);
	if (got <= 0) {
		return false;
	}
	ks_write_success(&sink->buffer, got);
	hot_len += got;
	return true;
}
int KDechunkContext::Read(KHttpSink* sink, char* buf, int length)
{
	if (chunk_left > 0) {
		return ReadChunk(sink, buf, length);
	}
	if (hot == NULL) {
		hot = sink->buffer.buf;
		hot_len = sink->buffer.used;
	}
	if (hot_len > 0) {
		return ParseBuffer(sink, buf, length);
	}
	sink->buffer.used = 0;
	hot = sink->buffer.buf;
	if (!ReadDataFromNet(sink)) {
		return -1;
	}
	return ParseBuffer(sink, buf, length);
}
/*
kev_result KDechunkContext::Read(KHttpSink *sink, void *arg, result_callback result, buffer_callback buffer)
{
	this->arg = arg;
	this->result = result;
	this->buffer = buffer;
	if (chunk_left > 0) {
		return ReadChunk(sink);
	}
	if (hot==NULL) {
		hot = sink->buffer.buf;
		hot_len = sink->buffer.used;
	}
	if (hot_len > 0) {
		return ParseBuffer(sink);
	}
	sink->buffer.used = 0;
	hot = sink->buffer.buf;
	return selectable_read(&sink->cn->st, result_dechunk_context, buffer_read_http_sink2, sink);
}
*/
int KDechunkContext::ParseBuffer(KHttpSink *sink, char* buffer, int length)
{
	kassert(chunk_left == 0);
	for (;;) {
		dechunk_status status = eng.dechunk(&hot, hot_len, &chunk, chunk_left);
		if (chunk && buffer) {
			return ReadChunk(sink, buffer, length);
		}
		switch (status) {
		case dechunk_failed:
			return -1;
		case dechunk_end:
			chunk_left = 0;
			return ReadChunk(sink, buffer, length);
		case dechunk_continue:
			chunk_left = 0;
			sink->buffer.used = 0;
			hot = sink->buffer.buf;
			hot_len = 0;
			if (!ReadDataFromNet(sink)) {
				return -1;
			}
			break;
		default:
			break;
		}
	}
	return kev_err;
}
int KDechunkContext::ReadChunk(KHttpSink* sink, char* buffer, int length)
{
	kassert(sink->buffer.used >= chunk_left);
	if (chunk_left == 0) {
		ks_save_point(&sink->buffer, hot, hot_len);
		hot_len = 0;
		hot = NULL;
		return 0;
	}
	if (buffer) {
		length = MIN(length, chunk_left);
		if (length > 0) {
			kgl_memcpy(buffer, chunk, length);
			chunk_left -= length;
			chunk += length;
		}
	} else {
		length = chunk_left;
		chunk_left = 0;
	}
	return length;
}
#if 0
kev_result KDechunkContext::ReadChunk(KHttpSink *sink)
{
	kassert(sink->buffer.used >= chunk_left);
	int len;
	if (chunk_left == 0) {
		ks_save_point(&sink->buffer, hot, hot_len);
		hot_len = 0;
		hot = NULL;
		return result(sink->GetOpaque(), arg, 0);
	}
	if (buffer) {
		WSABUF buf;
		int bc = buffer(sink->GetOpaque(), arg, &buf, 1);
		kassert(bc == 1);
		len = MIN((int)buf.iov_len, chunk_left);
		if (len > 0) {
			kgl_memcpy(buf.iov_base, chunk, len);
			chunk_left -= len;
			chunk += len;
		}
	} else {
		len = chunk_left;
		chunk_left = 0;
	}
	return result(sink->GetOpaque(), arg, len);
}
#endif
