/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef request_h_include
#define request_h_include
#include <string.h>
#include <map>
#include <list>
#include <string>
#include "global.h"
#include "ksocket.h"
#include "KMutex.h"
#include "KAcserver.h"
#include "KHttpHeaderManager.h"
#include "KBuffer.h"
#include "KAutoBuffer.h"
#include "KReadWriteBuffer.h"
#include "KStringBuf.h"
#include "KHttpAuth.h"
#include "do_config.h"
#include "KContext.h"
#include "KResponseContext.h"
#include "KUrl.h"
#include "KFileName.h"
#include "KSpeedLimit.h"
#include "KTempFile.h"
#include "kserver.h"
#include "KFlowInfo.h"
#include "KHttp2.h"
#include "kmalloc.h"
#include "kconnection.h"
#include "KHttpParser.h"
#include "KHttpLib.h"
#include "KFetchObject.h"
#include "KRequestQueue.h"
#include "KSockPoolHelper.h"
#include "KSink.h"


#define KGL_REQUEST_POOL_SIZE 4096
#ifdef ENABLE_STAT_STUB
extern volatile uint64_t kgl_total_requests;
extern volatile uint64_t kgl_total_accepts;
extern volatile uint64_t kgl_total_servers;
extern volatile uint32_t kgl_reading;
extern volatile uint32_t kgl_writing;
extern volatile uint32_t kgl_waiting;
#endif
#define READ_BUFF_SZ	8192

class KFetchObject;
class KSubVirtualHost;
class KFilterHelper;
class KHttpObject;
class KAccess;
class KFilterKey;
class KRequestQueue;
class KBigObjectContext;
class KSink;
class KHttpTransfer;
#define		REQUEST_EMPTY	0
#define		REQUEST_READY	1
#define 	MIN_SLEEP_TIME	4

void log_access(KHttpRequest* rq);
kev_result on_sink_readhup(KOPAQUE data, void* arg, int got);
KHttpHeaderIteratorResult handle_http_header(void* arg, KHttpHeader* header);
class KManageIP
{
public:
	KMutex ip_lock;
	std::map<ip_addr, unsigned> ip_map;
};

class KContext;
class KOutputFilterContext;
class KHttpFilterContext;
class KHttpRequest;
struct kgl_sub_request {
	kgl_request_range* range;
	kgl_precondition* precondition;
};
class KHttpRequestData
{
public:
	KHttpRequestData() {
		memset(this, 0, sizeof(*this));
	}
	struct {
		uint32_t filter_flags;
		uint32_t internal:1;
		uint32_t simulate : 1;
		uint32_t skip_access : 1;
		uint32_t cache_hit_part : 1;
		uint32_t have_stored : 1;
		uint32_t upstream_connection_keep_alive : 1;
		//connect代理
		//uint32_t connection_connect_proxy : 1;
		uint32_t no_http_header : 1;
		uint32_t always_on_model : 1;
		uint32_t response_checked : 1;
		uint32_t upstream_sign : 1;
		uint32_t parent_signed : 1;
		//client主动关闭
		uint32_t read_huped : 1;
		uint32_t upstream_expected_done : 1;
		//precondition类型
		uint32_t precondition_flag : 3;
		int64_t left_read;	//final fetchobj 使用
		KHttpObject* obj;
		KHttpObject* old_obj;
		kgl_sub_request* sub_request;
		kgl_response_body body;
		kgl_request_body* in_body;
	} ctx;
	KFetchObject* fo_head;
	KFetchObject* fo_last;
	//物理文件映射
	KFileName* file;
	//http认证
	KHttpAuth* auth;
#ifdef ENABLE_REQUEST_QUEUE
	KRequestQueue* queue;
#endif
	//限速(叠加)
	KSpeedLimitHelper* slh;
	/****************
* 输出过滤
*****************/
	KOutputFilterContext* of_ctx;
	KOutputFilterContext* getOutputFilterContext();
	void pushSpeedLimit(KSpeedLimit* sl) {
		KSpeedLimitHelper* helper = new KSpeedLimitHelper(sl);
		helper->next = slh;
		slh = helper;
	}
	int get_sleep_msec(int len) {
		int msec = 0;
		KSpeedLimitHelper* helper = slh;
		while (helper) {
			int t = helper->sl->get_sleep_msec(len);
			if (t > msec) {
				msec = t;
			}
			helper = helper->next;
		}
		return msec;
	}

	void push_obj(KHttpObject* obj);
	void pop_obj();
	void dead_old_obj();
	void clean_obj();
protected:
	void destroy_source();
};
class KHttpRequest final : public KHttpRequestData
{
public:
	inline KHttpRequest(KSink* sink) {
		this->sink = sink;
	}
	~KHttpRequest() {
#ifdef ENABLE_REQUEST_QUEUE
		ReleaseQueue();
#endif
		assert(!file);
		assert(!slh);
		assert(!fo_head);
		assert(!of_ctx);
		/* be sure response_body is closed. */
		assert(ctx.body.ctx == nullptr);
		assert(ctx.in_body == nullptr);
	}
	KSink* sink;
	kgl_request_range* get_range() {
		if (ctx.sub_request) {
			return ctx.sub_request->range;
		}
		return sink->data.range;
	}
	kgl_sub_request* alloc_sub_request() {
		ctx.sub_request = (kgl_sub_request*)kgl_pnalloc(sink->pool, sizeof(kgl_sub_request));
		*ctx.sub_request = { 0 };
		ctx.precondition_flag = 0;
		return ctx.sub_request;
	}
	void store_obj();
	bool is_bad_url() {
		if (unlikely(sink->data.url == NULL || sink->data.url->is_bad() || sink->data.meth == METH_UNSET)) {
			return true;
		}
		return false;
	}
	void set_url_param(char* param) {
		assert(sink->data.url->param == NULL);
		sink->data.url->param = xstrdup(param);
	}
#ifdef ENABLE_TF_EXCHANGE
	bool NeedTempFile() {
		KFetchObject* fo = fo_head;
		while (fo) {
			if (KBIT_TEST(fo->flags, KGL_UPSTREAM_NEED_TMP_FILE)) {
				return true;
			}
			fo = fo->next;
		}
		return false;
	}
#endif
	KString getInfo();
	kgl_auto_cstr getUrl();
	void beginRequest() {
		if (sink->data.url->path) {
			KFileName::tripDir3(sink->data.url->path, '/');
		}
#ifdef MALLOCDEBUG
		if (quit_program_flag != PROGRAM_NO_QUIT) {
			KBIT_SET(sink->data.flags, RQ_CONNECTION_CLOSE);
		}
#endif
		sink->data.iterator(handle_http_header, this);
	}
	void EndRequest() {
		if (ctx.in_body) {
			ctx.in_body->f->close(ctx.in_body->ctx);
			ctx.in_body = nullptr;
		}
		sink->remove_readhup();
		sink->end_request();
		clean();
	}
	int64_t get_left() {
		return sink->data.left_read;
	}
	int read(char* buf, int len) {
		return sink->read(buf, len);
	}
	uint32_t get_upstream_flags() {
		uint32_t flags = 0;
		if (KBIT_TEST(sink->data.flags, RQ_UPSTREAM_ERROR)) {
			KBIT_SET(flags, KSOCKET_FLAGS_SKIP_POOL);
		}
		if (KBIT_TEST(sink->data.flags, RQ_HAS_CONNECTION_UPGRADE)) {
			KBIT_SET(flags, KSOCKET_FLAGS_WEBSOCKET);
		}
		return flags;
	}
	KFetchObject* get_next_source(KFetchObject* fo) {
		if (fo != fo_head) {
			assert(false);
			return NULL;
		}
		auto next = fo_head->next;
		delete fo_head;
		fo_head = next;
		if (!fo_head) {
			fo_last = nullptr;
		}
		return fo_head;
	}
	bool continue_next_source(KGL_RESULT result) {
		if (KBIT_TEST(sink->data.flags, RQ_HAS_SEND_HEADER | RQ_HAS_READ_POST | RQ_NEXT_CALLED)) {
			//has touched input/output stream.
			//or open_next called.
			return false;
		}
		return result == KGL_NEXT;
	}
	int getFollowLink() {
		int follow_link = 0;
		if (conf.path_info) {
			follow_link |= FOLLOW_PATH_INFO;
		}
		if (KBIT_TEST(ctx.filter_flags, RF_FOLLOWLINK_OWN)) {
			follow_link |= FOLLOW_LINK_OWN;
			return follow_link;
		}
		if (KBIT_TEST(ctx.filter_flags, RF_FOLLOWLINK_ALL)) {
			follow_link |= FOLLOW_LINK_ALL;
		}
		return follow_link;
	}
	void parse_connection(const char* val, const char* end);
	void readhup() {
		if (!conf.read_hup) {
			return;
		}
		sink->readhup(this, on_sink_readhup);
	}

	bool rewrite_url(const char* newUrl, int errorCode = 0, const char* prefix = NULL);

	void LeaveRequestQueue() {
		sink->set_state(STATE_SEND);
	}
	void EnterRequestQueue() {
		sink->set_state(STATE_WAIT);
	}

	kgl_auto_cstr map_url_path(const char* url, KBaseRedirect* caller);
	inline bool response_status(uint16_t status_code) {
		return sink->response_status(status_code);
	}
	bool response_content_range(kgl_request_range* range, int64_t content_length) {
		KStringBuf s;
		s.WSTR("bytes ");
		if (range) {
			s.add(range->from, INT64_FORMAT);
			s.WSTR("-");
			s.add(range->to, INT64_FORMAT);
			s.WSTR("/");
		} else {
			s.WSTR("*/");
		}
		s.add(content_length, INT64_FORMAT);
		return response_header(kgl_header_content_range, s.buf(), s.size());
	}
	inline bool response_header(kgl_header_type name, const char* val, hlen_t val_len, bool lock_value = false) {
		return sink->response_header(name, val, val_len, lock_value);
	}
	inline bool response_header(kgl_header_type name, int64_t value) {
		return sink->response_header(name, value);
	}
	inline bool response_header(const char* name, hlen_t name_len, int val) {
		char buf[16];
		int len = snprintf(buf, sizeof(buf) - 1, "%d", val);
		return response_header(name, name_len, buf, len);
	}
	bool response_last_modified(time_t last_modified) {
		char* tmpbuf = (char*)alloc_request_memory(KGL_1123_TIME_LEN + 1);
		char* end = make_http_time(last_modified, tmpbuf, KGL_1123_TIME_LEN);
		return response_header(kgl_header_last_modified, tmpbuf, (int)(end - tmpbuf), true);
	}
#if 0
	bool response_content_length(int64_t content_length) {
		return sink->response_content_length(content_length);
	}
#endif
	//返回true，一定需要回应content-length或chunk
	//inline bool response_connection() {
	//	return sink->response_connection();
	//}

	/**
	* if lock_header true the header param will locked by sink until startResponseBody be called.
	*/
	bool response_header(KHttpHeader* header, bool lock_header) {
		if (header->name_is_know) {
			assert(header->know_header < kgl_header_unknow);
			//use KGL_HEADER_VALUE_CONST will lock the header until startResponseBody called
			return sink->response_header((kgl_header_type)header->know_header, header->buf + header->val_offset, header->val_len, lock_header);
		}
		return sink->response_header(header->buf, header->name_len, header->buf + header->val_offset, header->val_len);
	}
	bool response_header(const char* name, hlen_t name_len, const char* val, hlen_t val_len) {
		return sink->response_header(name, name_len, val, val_len);
	}
	//发送完header开始发送body时调用
	bool start_response_body(INT64 body_len) {
		return sink->start_response_body(body_len);
	}
	inline bool needFilter() {
		return of_ctx != NULL;
	}
	void response_vary(const char* vary);
	kgl_auto_cstr build_vary(const char* vary);
	const char* get_method() {
		return KHttpKeyValue::get_method(sink->data.meth)->data;
	}
	const kgl_str_t* get_method2() {
		return KHttpKeyValue::get_method(sink->data.meth);
	}

	/* override KWStream function begin */
	/*
	KGL_RESULT write_all(WSABUF* buf, int vc);
	KGL_RESULT write_all(const char* buf, int len);
	*/
	int write(const char* buf, int len) {
		auto msec = get_sleep_msec(len);
		if (msec > 0) {
			kfiber_msleep(msec);
		}
		return sink->write_all(buf, len);
	}
	int write(const kbuf* buf, int len) {
		if (!slh) {
			return sink->write_all(buf, len);
		}
		while (len > 0) {
			int got = KGL_MIN(len, buf->used);
			auto msec = get_sleep_msec(got);
			if (msec > 0) {
				kfiber_msleep(msec);
			}
			len -= got;
			int left = sink->write_all(buf->data, got);
			if (left > 0) {
				return len + left;
			}
			buf = buf->next;
		}
		return len;
	}
	KGL_RESULT write_end(KGL_RESULT result) {
		assert(ctx.body.ctx);
		if (result == KGL_OK && sink->get_response_left() > 0) {
			//有content-length，又未读完
			result = KGL_ESOCKET_BROKEN;
		}
		ctx.body = { 0 };
		return result;
	}
	KGL_RESULT sendfile(KASYNC_FILE fp, int64_t* total_length) {
		auto max_packet = conf.io_buffer;
		if (slh && max_packet > NBUFF_SIZE) {
			max_packet = NBUFF_SIZE;
		}
		while (*total_length > 0) {
			int length = (int)KGL_MIN(*total_length, max_packet);
			auto msec = get_sleep_msec(length);
			if (msec > 0) {
				kfiber_msleep(msec);
			}
			do {
				int got = sink->sendfile((kasync_file*)fp, length);
				if (got <= 0) {
					return KGL_EIO;
				}
				length -= got;
				*total_length -= got;
			} while (length > 0);
		}
		return KGL_OK;
	}

	KGL_RESULT write_buf(const kbuf* buf, int length) {
		if (write(buf, length) == 0) {
			return KGL_OK;
		}
		return KGL_EIO;
	}
	uint16_t GetSelfPort() {
		return sink->get_self_port();
	}
	void SetSelfPort(uint16_t port, bool ssl) {
		sink->set_self_port(port, ssl);
	}
	//客户真实ip(有可能被替换)
	const char* getClientIp() {
		return sink->get_client_ip();
	}
	void registerRequestCleanHook(kgl_cleanup_f cb, void* data) {
		assert(sink->pool);
		kgl_cleanup_add(sink->pool, cb, data);
	}
	void registerConnectCleanHook(kgl_cleanup_f cb, void* data) {
		kgl_cleanup_add(sink->get_connection_pool(), cb, data);
	}
	void pushFlowInfo(KFlowInfo* fi) {
		return sink->push_flow_info(fi);
	}
	uint32_t GetWorkModel() {
		return sink->get_server_model();
	}
	/* 数据源 */
	bool is_source_empty() {
		return fo_head == nullptr;
	}
	KFetchObject* get_source() {
		return fo_head;
	}

	void insert_source(KFetchObject* fo) {
		fo->next = fo_head;
		if (!fo_last) {
			fo_last = fo;
		}
		fo_head = fo;
	}
	void append_source(KFetchObject* fo);
	bool has_final_source() {
		if (!fo_last) {
			return false;
		}
		return !KBIT_TEST(fo_last->flags, KGL_UPSTREAM_FILTER);
	}
	/* 数据源结束 */

	//从堆上分配内存，在rq删除时，自动释放。
	void* alloc_connect_memory(int size) {
		return kgl_pnalloc(sink->get_connection_pool(), size);
	}
	void* alloc_request_memory(int size) {
		return kgl_pnalloc(sink->pool, size);
	}
	bool get_http_value(const char* attr, size_t attr_len, kgl_str_t* result) {
		auto header = sink->data.find(attr, (int)attr_len);
		if (header == nullptr) {
			return false;
		}
		result->data = header->buf + header->val_offset;
		result->len = header->val_len;
		return true;
	}
	bool get_http_value(kgl_header_type attr, kgl_str_t* result) {
		auto header = sink->data.find(attr);
		if (header == nullptr) {
			return false;
		}
		result->data = header->buf + header->val_offset;
		result->len = header->val_len;
		return true;
	}

#ifdef ENABLE_REQUEST_QUEUE
	bool NeedQueue() {
		KFetchObject* fo = fo_head;
		while (fo) {
			if (KBIT_TEST(fo->flags, KGL_UPSTREAM_NEED_QUEUE)) {
				return true;
			}
			fo = fo->next;
		}
		return false;
	}
	void ReleaseQueue() {
		if (queue) {
			queue->release();
			queue = NULL;
		}
	}
#endif
	KGL_RESULT open_next(KFetchObject* fo, kgl_input_stream* in, kgl_output_stream* out, const char* queue);
	KFetchObject* replace_next(KFetchObject* fo, KFetchObject* next_fo);
private:
	void close_source() {
		destroy_source();
#ifdef ENABLE_REQUEST_QUEUE
		ReleaseQueue();
#endif
	}
	void clean();
};
struct RequestError
{
	int code;
	const char* msg;
	void set(int code, const char* msg) {
		this->code = code;
		this->msg = msg;
	}
};
inline u_short string_hash(const char* p, int len, u_short res) {
	while (*p && len > 0) {
		--len;
		res = res * 3 + (*p);
		p++;
	}
	return res;
}
inline u_short string_hash(const char* p, u_short res) {
	int i = 8;
	while (*p && i) {
		--i;
		res *= *p;
		p++;
	}
	return res;
}
#endif
