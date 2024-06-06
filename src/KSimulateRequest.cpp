#include "KSimulateRequest.h"
#include "KHttpRequest.h"
#include "http.h"
#include "kselector_manager.h"
#include "KHttpProxyFetchObject.h"
#include "KDynamicListen.h"
#include "KTimer.h"
#include "KGzip.h"
#include "kmalloc.h"
#include "KVirtualHostManage.h"
#include "KAccessDsoSupport.h"
#include "HttpFiber.h"

#define ASYNC_DOWNLOAD_TMP_EXT ".tmp"
#ifdef ENABLE_SIMULATE_HTTP
int skip_access_request(void* arg, int got)
{
	KHttpRequest* rq = (KHttpRequest*)arg;
	int result = (int)fiber_http_start(rq);	
	if (kfiber_has_next()) {
		return result;
	}

	return stage_end_request(rq, (KGL_RESULT)result);
}

KHttpRequest *kgl_create_simulate_request(kgl_async_http *ctx)
{
	if (ctx->post_len > 0 && ctx->post == NULL) {
		return NULL;
	}

	kselector *selector = kgl_get_tls_selector();
	if (selector == NULL) {
		return NULL;
	}
	KSimulateSink* ss = new KSimulateSink;
	ss->data.raw_url = new KUrl;
	KHttpRequest* rq = new KHttpRequest(ss);
	selectable_bind(&ss->cn->st, selector);	
	selectable_bind_opaque(&ss->cn->st, rq);
	if (!parse_url(ctx->url, ss->data.raw_url)) {
		ss->data.raw_url->relase();
		ss->data.raw_url = new KUrl;
		KStringBuf nu;
		nu << ctx->url << "/";
		if (!parse_url(nu.c_str(), ss->data.raw_url)) {
			delete rq;
			return NULL;
		}
	}
	if (ss->data.raw_url->host == NULL) {
		delete rq;
		return NULL;
	}
	if (KBIT_TEST(ss->data.raw_url->flags, KGL_URL_ORIG_SSL)) {
		KBIT_SET(ss->data.raw_url->flags, KGL_URL_SSL);
	}

	if (ctx->host) {
		ss->host = strdup(ctx->host);
	}
	ss->arg = ctx->arg;
	ss->port = ctx->port;
	ss->life_time = ctx->life_time;
	
	ss->header = ctx->header;
	ss->header_finish = ctx->header_finish;
	ss->body = ctx->body;
	ss->post = ctx->post;

	KHttpHeader *head = ctx->rh;
	bool user_agent = false;
	while (head) {
		if (kgl_is_attr(head,_KS("User-Agent"))) {
			user_agent = true;
		}
		kgl_str_t attr;
		kgl_get_header_name(head, &attr);
		rq->sink->parse_header(attr.data, (int)attr.len, head->buf + head->val_offset, (int)head->val_len, false);
		head = head->next;
	}
	if (!user_agent) {
		//add default user-agent header
		timeLock.Lock();
		rq->sink->parse_header(_KS("User-Agent"), conf.serverName, conf.serverNameLength, false);
		timeLock.Unlock();
	}
	rq->sink = ss;
	rq->ctx.simulate = 1;
	if (KBIT_TEST(ctx->flags, KF_SIMULATE_GZIP)) {
		rq->sink->parse_header(_KS("Accept-Encoding"), _KS("gzip"), false);
	}
	rq->sink->data.meth = KHttpKeyValue::get_method(ctx->meth, (int)strlen(ctx->meth));
	rq->sink->data.left_read = ctx->post_len;
	rq->sink->data.set_http_version(1, 1);
	KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
	if (!KBIT_TEST(ctx->flags, KF_SIMULATE_CACHE)) {
		KBIT_SET(rq->sink->data.flags, RQ_HAS_NO_CACHE);
		KBIT_SET(rq->ctx.filter_flags, RF_NO_CACHE);
	}
	if (rq->sink->data.left_read > 0) {
		KBIT_SET(rq->sink->data.flags, RQ_HAS_CONTENT_LEN);
	}
	if (KBIT_TEST(ctx->flags, KF_SIMULATE_LOCAL)) {
		ss->cn->server = conf.gvm->RefsServer(rq->sink->data.raw_url->port);
		if (ss->cn->server == NULL) {
			ss->body = NULL;
			delete rq;
			return NULL;
		}
	} else {
		if (ctx->queue) {			
			rq->queue = get_request_queue(rq, ctx->queue);
		}
		rq->ctx.skip_access = 1;
	}
	ss->read_header();
	return rq;
}
int kgl_start_simulate_request(KHttpRequest *rq,kfiber **fiber)
{
	if (rq->ctx.skip_access) {
		rq->beginRequest();
		if (!rq->has_final_source()) {
			rq->append_source(new KHttpProxyFetchObject());
		}
		return kfiber_create(skip_access_request, rq, 0, http_config.fiber_stack_size, fiber);
	}
	return 0;
}
int kgl_simuate_http_request(kgl_async_http *ctx,kfiber **fiber)
{
	KHttpRequest *rq = kgl_create_simulate_request(ctx);
	if (rq == NULL) {
		return -1;
	}
	return kgl_start_simulate_request(rq,fiber);
}
int WINAPI test_header_hook(void *arg,int code,KHttpHeader *header)
{
	return 0;
}
int WINAPI test_body_hook(void *arg,const char *data,int len)
{
	if (data) {
		fwrite(data, 1, len, stdout);
	}
	return 0;
}
int WINAPI test_post_hook(void *arg,char *buf,int len)
{
	kgl_memcpy(buf,"test",4);
	return 4;
}
static void WINAPI timer_simulate(void *arg)
{
	kgl_async_http ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.url = "http://www.baidu.com/";
	ctx.meth = "get";
	ctx.post_len = 0;
	ctx.flags = 0;
	ctx.body = test_body_hook;
	ctx.arg = NULL;
	ctx.rh = NULL;
	ctx.post = test_post_hook;
	kgl_simuate_http_request(&ctx);
	//asyncHttpRequest(METH_GET,"http://www.kangleweb.net/test.php",NULL,test_header_hook,test_body_hook,NULL);
}
kev_result KSimulateSink::read_header() {
	begin_request();
	return kev_ok;
}
KSimulateSink::KSimulateSink() : KSingleConnectionSink(NULL, NULL)
{
	status_code = 0;
	header = NULL;
	header_finish = NULL;
	body = NULL;
	post = NULL;	
	arg = NULL;
	life_time = 0;
	port = 0;
	sockaddr_i addr;
	ksocket_getaddr("127.0.0.1", 0, PF_INET, AI_NUMERICHOST, &addr);
	cn = kconnection_new(&addr);
}
KSimulateSink::~KSimulateSink()
{
	if (body) {
		this->body(arg, NULL, response_left==0);
	}
}
int KSimulateSink::end_request()
{
	delete this;
	return 0;
}

typedef struct {
	KString save_file;
	time_t last_modified;
	int code;
	bool gzip;
	KWStream* st;
} async_download_worker;
int WINAPI async_download_header_finish(void* arg, uint16_t status_code, int64_t body_size) {
	async_download_worker* dw = (async_download_worker*)arg;
	dw->code = status_code;
	if (status_code == STATUS_OK) {
		KFileStream* fs = new KFileStream;
		fs->last_modified = dw->last_modified;
		KStringBuf filename;
		filename << dw->save_file << ASYNC_DOWNLOAD_TMP_EXT;
		if (!fs->open(filename.c_str())) {
			delete fs;
			return 1;
		}
		dw->st = fs;
		if (dw->gzip) {
			KGzipDecompress* st = new KGzipDecompress(false, dw->st, true);
			dw->st = st;
		}
	}
	return 0;
}
int WINAPI async_download_header_hook(void *arg, const char *name,int name_len,const char *val,int val_len)
{
	async_download_worker *dw = (async_download_worker *)arg;
	if (kgl_is_attr(name,name_len, _KS("Content-Encoding")) && kgl_mem_case_same(val, val_len, kgl_expand_string("gzip"))) {
		dw->gzip = true;
		return 0;
	}
	if (kgl_is_attr(name, name_len, _KS("Last-Modified"))) {
		dw->last_modified = kgl_parse_http_time((u_char*)val, val_len);
		return 0;
	}
	return 0;
}
int WINAPI async_download_body_hook(void *arg, const char *data, int len)
{
	async_download_worker *dw = (async_download_worker *)arg;
	if (data == NULL) {
		if (dw->st) {
			KGL_RESULT ret = dw->st->write_end(KGL_OK);
			delete dw->st;
			dw->st = NULL;
			KStringBuf filename;
			filename << dw->save_file << ASYNC_DOWNLOAD_TMP_EXT;
			if (ret==KGL_OK && len == 1 && (dw->code==200 || dw->code==206)) {
				unlink(dw->save_file.c_str());
				if (0 != rename(filename.c_str(), dw->save_file.c_str())) {
					dw->code += 2000;
				}
			} else {
				unlink(filename.c_str());
			}
		}
		if (len == 0 && (dw->code>=200 && dw->code<300)) {
			dw->code += 1000;
		}
	}
	if (data && dw->st) {
		if (KGL_OK != dw->st->write_all(data, len)) {
			return 1;
		}
	}
	return 0;
}

int kgl_async_download(const char *url, const char *file, int *status)
{
	assert(!kfiber_is_main());
	async_download_worker download_ctx;
	memset(&download_ctx, 0, sizeof(async_download_worker));
	download_ctx.save_file = file;
	kgl_async_http ctx;
	memset(&ctx, 0, sizeof(ctx));
	struct stat buf;
	int ret = stat(file, &buf);
	char tmp_buf[42];
	KHttpHeader *header = nullptr;	
	if (ret == 0) {
		mk1123time(buf.st_mtime, tmp_buf, 41);
		header = new_http_header(_KS("If-Modified-Since"), tmp_buf, 41);
		ctx.rh = header;
	}
	ctx.arg = &download_ctx;
	ctx.url = (char *)url;
	ctx.meth = "GET";
	ctx.post_len = 0;
	ctx.flags = KF_SIMULATE_GZIP;
	ctx.header = async_download_header_hook;
	ctx.header_finish = async_download_header_finish;
	ctx.body = async_download_body_hook;
	ctx.post = NULL;
	kfiber* fiber = NULL;
	ret = kgl_simuate_http_request(&ctx, &fiber);
	if (ret != 0) {
		*status = 0;
		goto clean;
	}
	kfiber_join(fiber, &ret);
	*status = download_ctx.code;
clean:
	if (header) {
		xfree_header(header);
	}
	return ret;
}
void async_download(const char* url, const char* file, result_callback cb, void* arg)
{
	int status;
	int got = kgl_async_download(url, file, &status);
	cb(NULL, arg, status);
}
kev_result test_simulate_callback(KOPAQUE data, void *arg, int code)
{
	printf("status_code=[%d]\n", code);
	return kev_err;
}
void test_simulate_local()
{
	kgl_async_http ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.url = (char *)"http://localhost/";
	ctx.meth = (char *)"get";
	ctx.post_len = 0;
	ctx.flags = KF_SIMULATE_LOCAL | KF_SIMULATE_GZIP;
	ctx.body = test_body_hook;
	ctx.arg = NULL;
	ctx.rh = NULL;
	ctx.post = NULL;
	kgl_simuate_http_request(&ctx);
}
void test_simulate_proxy()
{
	kgl_async_http ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.url = "https://www.cdnbest.com/public/view/default/js/flash.js";
	ctx.meth = "get";
	ctx.post_len = 0;
	//ctx.header = test_header_hook;
	ctx.flags = 0;
	ctx.body = test_body_hook;
	ctx.arg = NULL;
	ctx.rh = NULL;
	ctx.post = NULL;
	kgl_simuate_http_request(&ctx);
}
int test_simulate_fiber(void* arg, int got)
{
	//test_simulate_proxy();
	//int code;
	//kgl_async_download("https://www.cdnbest.com/public/view/default/js/global.js", "d:\\test.js",&code);
	//printf("code=[%d]\n", code);
	return 0;
}
bool test_simulate_request()
{
	//kfiber_create2(get_perfect_selector(), test_simulate_fiber, NULL, 0, 0, NULL);	
	//async_download("http://127.0.0.1:4411/range?g=1", "d:\\test.gz", test_simulate_callback, NULL);
	
	//return true;
	timer_run(timer_simulate, NULL, 0, 0);
	return true;
}
#endif

