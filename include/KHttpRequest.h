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
#include "KDomainUser.h"
#include "KSendable.h"
#include "KStringBuf.h"
#include "KHttpAuth.h"
#include "do_config.h"
#include "KContext.h"
#include "KResponseContext.h"
#include "KUrl.h"
#include "KFileName.h"
#include "KSpeedLimit.h"
#include "KTempFile.h"
#include "KInputFilter.h"
#include "kserver.h"
#include "KFlowInfo.h"
#include "KHttp2.h"
#include "kmalloc.h"
#include "kconnection.h"
#include "KHttpParser.h"
#include "KHttpLib.h"
#include "KFetchObject.h"
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

class KHttpRequest
{
public:
	inline KHttpRequest(KSink* sink) {
		memset(this, 0, sizeof(*this));
		ctx = new KContext;
		this->sink = sink;
	}
	~KHttpRequest();
	bool isBad();
	void set_url_param(char* param);
	//判断是否还有post数据可读
	bool has_post_data(kgl_input_stream* in);
	bool NeedTempFile(bool upload);
	std::string getInfo();
	char* getUrl();
	void beginRequest();
	int EndRequest();
	uint32_t get_upstream_flags();
	KFetchObject* get_next_source(KFetchObject* fo) {
		if (fo != fo_head) {
			delete fo;
			assert(false);
			return fo_head;
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
		if (KBIT_TEST(filter_flags, RF_FOLLOWLINK_OWN)) {
			follow_link |= FOLLOW_LINK_OWN;
			return follow_link;
		}
		if (KBIT_TEST(filter_flags, RF_FOLLOWLINK_ALL)) {
			follow_link |= FOLLOW_LINK_ALL;
		}
		return follow_link;
	}
	void parse_connection(const char* val, const char* end);
	void readhup();
	void close_source();
	bool rewrite_url(const char* newUrl, int errorCode = 0, const char* prefix = NULL);
	void EnterRequestQueue();
	void LeaveRequestQueue();
	char* map_url_path(const char* url, KBaseRedirect* caller);
	uint32_t filter_flags;
	KSink* sink;
	//数据源
	bool is_source_empty() {
		return fo_head == nullptr;
	}
	KFetchObject* get_source() {
		return fo_head;
	}
	KSubVirtualHost* get_virtual_host();
	KFetchObject* fo_head;
	KFetchObject* fo_last;
	//物理文件映射
	KFileName* file;
	KHttpTransfer* tr;
	//http认证
	KHttpAuth* auth;
	//有关object及缓存上下文
	KContext* ctx;
	//发送上下文
#ifdef ENABLE_INPUT_FILTER
	bool hasInputFilter() {
		if (if_ctx == NULL) {
			return false;
		}
		return !if_ctx->isEmpty();
	}
	/************
	* 输入过滤
	*************/
	KInputFilterContext* if_ctx;
	KInputFilterContext* getInputFilterContext() {
		if (if_ctx == NULL && (sink->data.content_length > 0 || sink->data.url->param)) {
			if_ctx = new KInputFilterContext(this);
		}
		return if_ctx;
	}
#endif
	/****************
	* 输出过滤
	*****************/
	KOutputFilterContext* of_ctx;
	KOutputFilterContext* getOutputFilterContext();
	void addFilter(KFilterHelper* chain);
	inline bool response_status(uint16_t status_code) {
		return sink->response_status(status_code);
	}
	inline bool response_header(kgl_header_type name, const char* val, hlen_t val_len, bool lock_value = false) {
		return sink->response_header(name, val, val_len, lock_value);
	}
	inline bool response_header(const char* name, hlen_t name_len, int val) {
		char buf[16];
		int len = snprintf(buf, sizeof(buf) - 1, "%d", val);
		return response_header(name, name_len, buf, len);
	}
	bool response_content_length(int64_t content_length) {
		return sink->response_content_length(content_length);
	}
	//返回true，一定需要回应content-length或chunk
	inline bool response_connection() {
		return sink->response_connection();
	}
	KGL_RESULT sendfile(kfiber_file* fp, int64_t *len);
	/**
	* if lock_header true the header param will locked by sink until startResponseBody be called.
	*/
	bool response_header(KHttpHeader* header, bool lock_header);
	bool response_header(const char* name, hlen_t name_len, const char* val, hlen_t val_len);
	//发送完header开始发送body时调用
	bool start_response_body(INT64 body_len);
	inline bool needFilter() {
		return of_ctx != NULL;
	}
	void ResponseVary(const char* vary);
	char* BuildVary(const char* vary);
	const char* get_method();
	bool write_all(WSABUF* buf, int* vc);
	int Write(WSABUF* buf, int bc);
	int Write(const char* buf, int len);
	bool write_all(const char* buf, int len);
	bool write_buff(kbuf* buf);
	int checkFilter(KHttpObject* obj);
	//限速(叠加)
	KSpeedLimitHelper* slh;
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
	uint16_t GetSelfPort() {
		return sink->get_self_port();
	}
	void SetSelfPort(uint16_t port, bool ssl);
	//客户真实ip(有可能被替换)
	const char* getClientIp() {
		return sink->get_client_ip();
	}
	void registerRequestCleanHook(kgl_cleanup_f callBack, void* data) {
		assert(sink->pool);
		kgl_pool_cleanup_t* cn = kgl_pool_cleanup_add(sink->pool, 0);
		cn->data = data;
		cn->handler = callBack;
	}
	void registerConnectCleanHook(kgl_cleanup_f callBack, void* data) {
		kgl_pool_cleanup_t* cn = kgl_pool_cleanup_add(sink->get_connection_pool(), 0);
		cn->data = data;
		cn->handler = callBack;
	}
	void pushFlowInfo(KFlowInfo* fi) {
		return sink->push_flow_info(fi);
	}
	uint32_t GetWorkModel() {
		return sink->get_server_model();
	}
	void insert_source(KFetchObject* fo);
	void append_source(KFetchObject* fo);
	bool has_final_source();
	bool has_before_cache();
#ifdef ENABLE_BIG_OBJECT_206
	//大物件上下文
	KBigObjectContext* bo_ctx;
#endif

#ifdef ENABLE_REQUEST_QUEUE
	KRequestQueue* queue;
#endif
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
	int Read(char* buf, int len);
#ifdef ENABLE_REQUEST_QUEUE
	bool NeedQueue();
	void ReleaseQueue();
#endif
	KGL_RESULT open_next(KFetchObject* fo, kgl_input_stream* in, kgl_output_stream* out, const char* queue);
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
