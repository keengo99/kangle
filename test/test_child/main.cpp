#include <stdio.h>
#include "KHttpServer.h"
#include "kselector_manager.h"
#include "kfiber.h"
#include "KSink.h"
#include "klog.h"
#include "kforwin32.h"
#include "fastcgi.h"

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
int fastcgi_fiber(void* arg, int len) {
	klog(KLOG_NOTICE, "fastcgi_handle...\n");
	kconnection* cn = (kconnection*)arg;
	FCGI_BeginRequestRecord record;
	len = sizeof(record);
	if (!kfiber_net_read_full(cn, (char*)&record, &len)) {
		klog(KLOG_ERR, "cann't read begin record len=[%d]\n", len);
		kconnection_destroy(cn);
		return kev_destroy;
	}
	klog(KLOG_NOTICE, "success read beginrequest\n");
	char resp_buf[512];
	int resp_len = snprintf(resp_buf, sizeof(resp_buf), "Status: 200 OK\r\nCache-Control: no-cache\r\n\r\nhello %d %d", getpid(), (int)time(NULL));
	char pad_buf[512];
	char* body = NULL;
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
		if (header.type == FCGI_STDIN) {
			//TODO: parse
			if (header.contentLength == 0) {
				//TODO: stdin end
				FCGI_Header resp_header;
				memset(&resp_header, 0, sizeof(resp_header));
				resp_header.version = 1;
				resp_header.requestId = header.requestId;
				resp_header.type = FCGI_STDOUT;
				resp_header.contentLength = htons((uint16_t)resp_len);
				int header_len = sizeof(resp_header);
				if (!kfiber_net_write_full(cn, (char*)&resp_header, &header_len)) {
					goto err;
				}
				if (!kfiber_net_write_full(cn, resp_buf, &resp_len)) {
					goto err;
				}
				resp_header.contentLength = 0;
				header_len = sizeof(resp_header);
				if (!kfiber_net_write_full(cn, (char*)&resp_header, &header_len)) {
					goto err;
				}
				resp_header.type = FCGI_END_REQUEST;
				resp_header.contentLength = htons(sizeof(FCGI_EndRequestBody));
				header_len = sizeof(resp_header);
				if (!kfiber_net_write_full(cn, (char*)&resp_header, &header_len)) {
					goto err;
				}
				FCGI_EndRequestBody end_body;
				memset(&end_body, 0, sizeof(end_body));
				header_len = sizeof(end_body);
				if (!kfiber_net_write_full(cn, (char*)&resp_header, &header_len)) {
					goto err;
				}
				goto err;
			}
		}
		if (body) {
			free(body);
			body = NULL;
		}
	}
	return 0;
err:
	kconnection_destroy(cn);
	if (body) {
		free(body);
	}
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
