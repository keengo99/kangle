#include <stdio.h>
#include <map>
#include "KHttpServer.h"
#include "kselector_manager.h"
#include "kfiber.h"
#include "KSink.h"
#include "klog.h"
#include "kforwin32.h"
#include "fastcgi.h"
#include "KHttpLib.h"
#include "utils.h"
#include "KEnvInterface.h"
#include "KFastcgiUtils.h"
/*
struct lessp_icase
{
	bool operator()(const char* __x, const char* __y) const {
		return strcasecmp(__x, __y) < 0;
	}
};
*/
class KMapEnv : public KEnvInterface
{
public:
	~KMapEnv()
	{
		std::map<char*, char*, lessp_icase >::iterator it;
		for (it = envs.begin(); it != envs.end(); it++) {
			xfree((*it).first);
			xfree((*it).second);
		}
	}
	const char* get_env(const char* attr)
	{
		std::map<char*, char*, lessp_icase >::iterator it;
		it = envs.find((char*)attr);
		if (it != envs.end()) {
			return (*it).second;
		}
		return NULL;
	}
	bool add_env(const char* attr, size_t attr_len, const char* val, size_t val_len) override
	{
		char* attr2 = kgl_strndup(attr, attr_len);
		std::map<char*, char*, lessp_icase >::iterator it;
		it = envs.find(attr2);
		if (it != envs.end()) {
			xfree(attr2);
			return false;
		}
		envs.insert(std::pair<char*, char*>(attr2, kgl_strndup(val, val_len)));
		return true;
	}
	std::map<char*, char*, lessp_icase > envs;	
};

#ifdef _WIN32
HANDLE hEventSource = INVALID_HANDLE_VALUE;
#endif
static uint16_t port = 0;
bool fastcgi = false;
void LogEvent(int level, const char* pFormat, va_list pArg)
{
	vfprintf(stderr, pFormat, pArg);
#ifdef _WIN32
	TCHAR chMsg[512];
	LPTSTR lpszStrings[1];
	vsnprintf(chMsg, sizeof(chMsg), pFormat, pArg);
	lpszStrings[0] = chMsg;
	if (hEventSource == INVALID_HANDLE_VALUE) {
		hEventSource = RegisterEventSource(NULL, "test_child");
	}
	if (hEventSource != INVALID_HANDLE_VALUE) {
		ReportEvent(hEventSource, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, (LPCTSTR*)&lpszStrings[0], NULL);
	}
#endif
}

SOCKET get_stdin_socket()
{
#ifdef _WIN32
	STARTUPINFO si;
	GetStartupInfo(&si);
	return (SOCKET)si.hStdInput;
#else
	return 0;
#endif
}
static void krequest_start(KSink* sink, int header_len)
{
	klog(KLOG_NOTICE, "meth=[%d] path=[%s]\n", sink->data.meth, sink->data.raw_url->path);
	sink->response_status(200);
	sink->response_connection();
	char buf[128];
	int len = snprintf(buf, sizeof(buf), "hello %d %d", getpid(), (int)time(NULL));
	sink->response_content_length(len);
	sink->response_header(kgl_expand_string("Cache-Control"), kgl_expand_string("no-cache,no-store"));
	sink->start_response_body(len);
	sink->write_all(buf, len);
	sink->end_request();
}
FCGI_Header* new_fastcgi_header(u_char type, u_char pad_length)
{
	FCGI_Header* header = (FCGI_Header*)malloc(sizeof(FCGI_Header) + pad_length);
	memset(header, 0, sizeof(FCGI_Header));
	header->version = 1;
	header->type = type;
	header->requestId = 1;
	header->paddingLength = pad_length;
	return header;
}
bool fastcgi_end_request(kconnection* cn)
{
	bool result = false;
	FCGI_Header* resp_header = new_fastcgi_header(FCGI_END_REQUEST, rand());	
	resp_header->contentLength = htons(sizeof(FCGI_EndRequestBody));
	int header_len = sizeof(FCGI_Header) + resp_header->paddingLength;
	if (!kfiber_net_write_full(cn, (char*)resp_header, &header_len)) {
		goto out;
	}
	if (rand() % 2 == 0) {
		kfiber_msleep(50);
	}
	FCGI_EndRequestBody end_body;
	memset(&end_body, 0, sizeof(end_body));
	header_len = sizeof(end_body);
	if (!kfiber_net_write_full(cn, (char*)&resp_header, &header_len)) {
		goto out;
	}
	result = true;
out:
	free(resp_header);
	return result;
}
bool fastcgi_stdout(kconnection* cn, const char* buf, int len)
{
	bool result = false;
	FCGI_Header* resp_header = new_fastcgi_header(FCGI_STDOUT, rand());
	resp_header->contentLength = htons((uint16_t)len);
	int header_len = sizeof(resp_header) + resp_header->paddingLength;
	if (!kfiber_net_write_full(cn, (char*)resp_header, &header_len)) {
		goto out;
	}
	if (len > 0) {
		if (kfiber_net_write_full(cn, buf, &len)) {
			goto out;
		}
	}
	result = true;
out:
	free(resp_header);
	return result;
}
bool fastcgi_test_304_keep_conn(kconnection* cn, int request_count, KMapEnv* env)
{
	auto if_none_match = env->get_env("HTTP_IF_NONE_MATCH");
	if (if_none_match == NULL || strcmp(if_none_match, "fcgi_etag") != 0) {
		return fastcgi_stdout(cn, _KS("Status: 200 OK\r\nEtag: fcgi_etag\r\nContent-Length: 5\r\n\r\nhello"));	
	}
	return fastcgi_stdout(cn, _KS("Status: 304 Not Modified\r\n\r\n"));
}
bool handle_fastcgi(kconnection* cn, int request_count, KMapEnv* env)
{
	char resp_buf[512];
	int resp_len = snprintf(resp_buf, sizeof(resp_buf), "Status: 200 OK\r\nCache-Control: no-cache\r\n\r\nhello %d %d", getpid(), (int)time(NULL));
	int pos = sizeof("Status: 200 OK\r\nCache-");
	const char* request_uri = env->get_env("REQUEST_URI");
	klog(KLOG_ERR, "REQUEST_URI=[%s] keep_alive_count=[%d]\n", request_uri,request_count);
#if 0
	for (auto it = env->envs.begin(); it != env->envs.end(); it++) {
		klog(KLOG_ERR, "%s: %s\n", (*it).first, (*it).second);
	}
#endif
	if (strncmp(request_uri, _KS("/fastcgi/304")) == 0) {
		fastcgi_test_304_keep_conn(cn, request_count, env);
		//msleep cause end_request packet send delay for test.
		my_msleep(100);	
		goto done;
	}
	fastcgi_stdout(cn, resp_buf, pos);
	fastcgi_stdout(cn, resp_buf + pos, resp_len - pos);
done:
	fastcgi_stdout(cn, NULL, 0);
	return fastcgi_end_request(cn);
}
bool fastcgi_connect(kconnection* cn,int request_count) {	
	FCGI_BeginRequestRecord record;
	int len = sizeof(record);
	if (!kfiber_net_read_full(cn, (char*)&record, &len)) {
		klog(KLOG_ERR, "cann't read begin record len=[%d]\n", len);
		return false;
	}
	bool keep_conn = KBIT_TEST(record.body.flags, FCGI_KEEP_CONN);
	klog(KLOG_NOTICE, "success read beginrequest\n");
	char pad_buf[512];
	char* body = NULL;
	ks_buffer* param_buffer = nullptr;
	for (;;) {
		FCGI_Header header;
		len = sizeof(header);
		if (!kfiber_net_read_full(cn, (char*)&header, &len)) {
			goto err;
		}
		header.contentLength = ntohs(header.contentLength);

		if (header.contentLength > 0) {
			body = (char*)malloc(header.contentLength);
			len = header.contentLength;
			if (!kfiber_net_read_full(cn, body, &len)) {
				goto err;
			}
		}
		if (header.paddingLength > 0) {
			len = header.paddingLength;
			if (!kfiber_net_read_full(cn, pad_buf, &len)) {
				goto err;
			}
		}
		klog(KLOG_ERR, "fastcgi type=[%d] length=[%d]\n", header.type,header.contentLength);
		if (header.type == FCGI_PARAMS) {
			if (param_buffer == nullptr) {
				param_buffer = ks_buffer_new(4096);
			}
			ks_write_str(param_buffer, body, header.contentLength);
		}
		if (header.type == FCGI_STDIN) {	
			if (header.contentLength == 0) {
				KMapEnv env;
				KFastcgiParser parser;
				if (param_buffer) {
					char* data = param_buffer->buf;
					parser.parse_param(&env, (u_char*)data, (u_char*)data + param_buffer->used);					
				} else {
					klog(KLOG_ERR, "param_buffer is NULL\n");
				}
				const char* x_time = env.get_env("HTTP_X_TIME");
				if (x_time != nullptr) {
					assert(atoi(x_time) == request_count);					
				}
				handle_fastcgi(cn, request_count, &env);
				goto done;
			}
		}
		if (body) {
			free(body);
			body = NULL;
		}
	}
err:
	keep_conn = false;
done:
	if (param_buffer) {
		ks_buffer_destroy(param_buffer);
		param_buffer = nullptr;
	}
	if (body) {
		free(body);
	}
	return keep_conn;
}
int fastcgi_fiber(void* arg, int len) {
	klog(KLOG_NOTICE, "fastcgi_handle...\n");
	kconnection* cn = (kconnection*)arg;
	for (int n=0;;n++) {
		if (!fastcgi_connect(cn, n)) {
			klog(KLOG_NOTICE, "not keep connection\n");
			break;
		}
		klog(KLOG_NOTICE, "keep connection\n");
	}
	klog(KLOG_NOTICE, "destroy connection...\n");
	kconnection_destroy(cn);
	return 0;
}
KACCEPT_CALLBACK(fastcgi_handle) {
	kfiber_create(fastcgi_fiber, arg, got, 0, NULL);
	return kev_ok;
}
int fiber_main(void* arg, int argc)
{
	klog(KLOG_NOTICE, "test_child running fastcgi=[%d]...\n", (int)fastcgi);
	SOCKET s;
	ksocket_init(s);
	char** argv = (char**)arg;
	kserver* server = kserver_init();
	int result = 0;
	if (port > 0) {
		if (!kserver_bind(server, "127.0.0.1", port, NULL)) {
			klog(KLOG_ERR, "cann't open port=[%d]\n", port);
			result = -1;
			goto done;
		}
	} else {
		s = get_stdin_socket();
		ksocket_no_block(s);
		klog(KLOG_NOTICE, "stdin handle=[%x]\n", (int)s);
	}
	if (fastcgi) {
		if (ksocket_opened(s)) {
			if (!kserver_open_exsit(server, s, fastcgi_handle)) {
				result = -1;
			}
			goto done;
		}
		if (!kserver_open(server, 0, fastcgi_handle)) {
			result = -1;
		}
		goto done;
	}
	if (!start_http_server(server, 0, s)) {
		klog(KLOG_ERR, "cann't open socket for a server\n");
		result = -1;
	}
done:
	port = ksocket_addr_port(&server->addr);
	klog(KLOG_NOTICE, "test_child start port=[%d] result=[%d]\n", port, result);
	kserver_release(server);
	return result;
}
int main(int argc, char** argv)
{
	klog_init(LogEvent);
	kasync_init();
	selector_manager_init(1, true);
	if (argc > 1) {
		if (strchr(argv[1], 'f') != NULL) {
			fastcgi = true;
		}
	}
	if (argc > 2) {
		port = (uint16_t)atoi(argv[2]);
	}
	init_http_server_callback(NULL, krequest_start);
	http_config.time_out = 60;
	selector_manager_set_timeout(60, 60);
	kfiber_create2(get_perfect_selector(), fiber_main, (void*)argv, argc, 0, NULL);
	selector_manager_start(NULL, false);
	return 0;
}
