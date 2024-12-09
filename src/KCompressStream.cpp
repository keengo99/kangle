#include "KCompressStream.h"
#include "KHttpRequest.h"
#include "KHttpObject.h"
#include "KGzip.h"
#include "KBrotli.h"
#include "KZstd.h"

bool pipe_compress_stream(KHttpRequest* rq, KHttpObject* obj, int64_t content_len, kgl_response_body* body) {
	if (content_len >= 0 && content_len < conf.min_compress_length) {
		//太短
		return false;
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		//透传模式不压缩
		return false;
	}
	//如果obj标记为已经压缩过，或者标记了不用压缩，则不压缩数据
	if (obj->IsContentEncoding()) {
		return false;
	}
	//obj有多个引用,不压缩
	if (obj->refs > 1) {
		return false;
	}
	//status_code是206，表示是部分内容时也不压缩,或者是200回应，但用了url ranged技术
	//注：这种情况没有经过详细考证
	if (obj->data->i.status_code == STATUS_CONTENT_PARTIAL
		|| KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_RANGED)) {
		return false;
	}
	if (KBIT_TEST(obj->index.flags, FLAG_DEAD) && conf.only_compress_cache == 1) {
		return false;
	}
	if (!obj->need_compress) {
		return false;
	}
#ifdef ENABLE_ZSTD
	if (conf.zstd_level > 0 && KBIT_TEST(rq->sink->data.raw_url.accept_encoding, KGL_ENCODING_ZSTD)) {
		if (pipe_zstd_compress(conf.zstd_level, body)) {
			obj->AddContentEncoding(KGL_ENCODING_ZSTD, kgl_expand_string("zstd"));
			return true;
		}
	}
#endif
#ifdef ENABLE_BROTLI
	if (conf.br_level > 0 && KBIT_TEST(rq->sink->data.raw_url.accept_encoding, KGL_ENCODING_BR)) {		
		if (pipe_brotli_compress(conf.br_level, body)) {
			obj->AddContentEncoding(KGL_ENCODING_BR, kgl_expand_string("br"));
			return true;
		}
	}
#endif
	//客户端支持gzip压缩格式
	if (conf.gzip_level > 0 && KBIT_TEST(rq->sink->data.raw_url.accept_encoding, KGL_ENCODING_GZIP)) {
		if (pipe_gzip_compress(conf.gzip_level, body)) {
			obj->AddContentEncoding(KGL_ENCODING_GZIP, kgl_expand_string("gzip"));
			return true;
		}
	}
	return false;
}