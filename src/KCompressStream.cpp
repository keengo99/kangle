#include "KCompressStream.h"
#include "KHttpRequest.h"
#include "KHttpObject.h"
#include "KGzip.h"
#ifdef ENABLE_BROTLI
#include "KBrotli.h"
#endif

KCompressStream *create_compress_stream(KHttpRequest *rq, KHttpObject *obj, int64_t content_len)
{
	if (content_len >= 0 && content_len < conf.min_compress_length) {
		//太短
		return NULL;
	}
	if (KBIT_TEST(rq->sink->data.flags,RQ_CONNECTION_UPGRADE)) {
		//透传模式不压缩
		return NULL;
	}
	//如果obj标记为已经压缩过，或者标记了不用压缩，则不压缩数据
	if (obj->IsContentEncoding()) {
		return NULL;
	}
	//obj有多个引用,不压缩
	if (obj->refs > 1) {
		return NULL;
	}
	//status_code是206，表示是部分内容时也不压缩,或者是200回应，但用了url ranged技术
	//注：这种情况没有经过详细考证
	if (obj->data->status_code == STATUS_CONTENT_PARTIAL
		|| KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_RANGED)) {
		return NULL;
	}
	if (KBIT_TEST(obj->index.flags, FLAG_DEAD) && conf.only_compress_cache == 1) {
		return NULL;
	}
#ifdef ENABLE_BROTLI
	if (obj->need_compress && conf.br_level>0 && KBIT_TEST(rq->sink->data.raw_url->accept_encoding, KGL_ENCODING_BR)) {
		obj->AddContentEncoding(KGL_ENCODING_BR, kgl_expand_string("br"));
		return new KBrotliCompress();
	}
#endif
	//客户端支持gzip压缩格式
	if (obj->need_compress && conf.gzip_level>0 && KBIT_TEST(rq->sink->data.raw_url->accept_encoding, KGL_ENCODING_GZIP)) {
		obj->AddContentEncoding(KGL_ENCODING_GZIP, kgl_expand_string("gzip"));
		return new KGzipCompress(conf.gzip_level);
	}
	return NULL;
}