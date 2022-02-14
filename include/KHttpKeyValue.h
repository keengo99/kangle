/*
 * KHttpKeyValue.h
 *
 *  Created on: 2010-4-23
 *      Author: keengo
 */

#ifndef KHTTPKEYVALUE_H_
#define KHTTPKEYVALUE_H_
#include <stdio.h>
#include "global.h"
#include "KHttpHeader.h"
#include "kmalloc.h"

#define METH_UNSET      0
#define METH_OPTIONS    1
#define METH_GET        2
#define METH_HEAD       3
#define METH_POST       4
#define METH_PUT        5
#define METH_DELETE     6
#define METH_TRACE      7
#define METH_PROPFIND   8
#define METH_PROPPATCH  9
#define METH_MKCOL      10
#define METH_COPY       11
#define METH_MOVE       12
#define METH_LOCK       13
#define METH_UNLOCK     14
#define METH_ACL        15
#define METH_REPORT	    16
#define METH_VERSION_CONTROL  17
#define METH_CHECKIN    18
#define METH_CHECKOUT   19
#define METH_UNCHECKOUT 20
#define METH_SEARCH     21
#define METH_MKWORKSPACE 22
#define METH_UPDATE     23
#define METH_LABEL      24
#define METH_MERGE      25
#define METH_BASELINE_CONTROL 26
#define METH_MKACTIVITY 27
#define METH_CONNECT    28
#define METH_PURGE      29
#define METH_PATCH      30
#define MAX_METHOD      31

typedef struct {
	int key;
	const char *value;
} keyvalue;
class KHttpKeyValue {
public:
	KHttpKeyValue();
	virtual ~KHttpKeyValue();
	static const char *getMethod(int meth);
	static int getMethod(const char *src);
	//static const char *getStatus(int status);

};
inline void getRequestLine(kgl_pool_t *pool,int status,kgl_str_t *ret)
{
	switch (status) {	
		case 100:kgl_str_set(ret, "HTTP/1.1 100 Continue\r\n");return;
		case 101:kgl_str_set(ret, "HTTP/1.1 101 Switching Protocols\r\n");return;
		case 102:kgl_str_set(ret, "HTTP/1.1 102 Processing\r\n");return; /* WebDAV */
		//2XX
		case 200:kgl_str_set(ret, "HTTP/1.1 200 OK\r\n");return;
		case 201:kgl_str_set(ret, "HTTP/1.1 201 Created\r\n");return;
		case 202:kgl_str_set(ret, "HTTP/1.1 202 Accepted\r\n");return;
		case 203:kgl_str_set(ret, "HTTP/1.1 203 Non-Authoritative Information\r\n");return;
		case 204:kgl_str_set(ret, "HTTP/1.1 204 No Content\r\n");return;
		case 205:kgl_str_set(ret, "HTTP/1.1 205 Reset Content\r\n");return;
		case 206:kgl_str_set(ret, "HTTP/1.1 206 Partial Content\r\n");return;
		case 207:kgl_str_set(ret, "HTTP/1.1 207 Multi-status\r\n");return; /* WebDAV */
		case 208:kgl_str_set(ret, "HTTP/1.1 208 Already Reported\r\n"); return;
		case 226:kgl_str_set(ret, "HTTP/1.1 226 IM Used\r\n"); return;
		//3XX
		case 300:kgl_str_set(ret, "HTTP/1.1 300 Multiple Choices\r\n");return;
		case 301:kgl_str_set(ret, "HTTP/1.1 301 Moved Permanently\r\n");return;
		case 302:kgl_str_set(ret, "HTTP/1.1 302 Found\r\n");return;
		case 303:kgl_str_set(ret, "HTTP/1.1 303 See Other\r\n");return;
		case 304:kgl_str_set(ret, "HTTP/1.1 304 Not Modified\r\n");return;
		case 305:kgl_str_set(ret, "HTTP/1.1 305 Use Proxy\r\n");return;
		case 306:kgl_str_set(ret, "HTTP/1.1 306 (Unused)\r\n");return;
		case 307:kgl_str_set(ret, "HTTP/1.1 307 Temporary Redirect\r\n");return;
		case 308:kgl_str_set(ret, "HTTP/1.1 308 Permanent Redirect\r\n"); return;
		//4XX
		case 400:kgl_str_set(ret, "HTTP/1.1 400 Bad Request\r\n");return;
		case 401:kgl_str_set(ret, "HTTP/1.1 401 Unauthorized\r\n");return;
		case 402:kgl_str_set(ret, "HTTP/1.1 402 Payment Required\r\n");return;
		case 403:kgl_str_set(ret, "HTTP/1.1 403 Forbidden\r\n");return;
		case 404:kgl_str_set(ret, "HTTP/1.1 404 Not Found\r\n");return;
		case 405:kgl_str_set(ret, "HTTP/1.1 405 Method Not Allowed\r\n");return;
		case 406:kgl_str_set(ret, "HTTP/1.1 406 Not Acceptable\r\n");return;
		case 407:kgl_str_set(ret, "HTTP/1.1 407 Proxy Authentication Required\r\n");return;
		case 408:kgl_str_set(ret, "HTTP/1.1 408 Request Timeout\r\n");return;
		case 409:kgl_str_set(ret, "HTTP/1.1 409 Conflict\r\n");return;
		case 410:kgl_str_set(ret, "HTTP/1.1 410 Gone\r\n");return;
		case 411:kgl_str_set(ret, "HTTP/1.1 411 Length Required\r\n");return;
		case 412:kgl_str_set(ret, "HTTP/1.1 412 Precondition Failed\r\n");return;
		case 413:kgl_str_set(ret, "HTTP/1.1 413 Request Entity Too Large\r\n");return;
		case 414:kgl_str_set(ret, "HTTP/1.1 414 Request-URI Too Long\r\n");return;
		case 415:kgl_str_set(ret, "HTTP/1.1 415 Unsupported Media Type\r\n");return;
		case 416:kgl_str_set(ret, "HTTP/1.1 416 Requested Range Not Satisfiable\r\n");return;
		case 417:kgl_str_set(ret, "HTTP/1.1 417 Expectation Failed\r\n");return;
		case 420:kgl_str_set(ret, "HTTP/1.1 420 Method Failure\r\n"); return;
		case 421:kgl_str_set(ret, "HTTP/1.1 421 Misdirected Request\r\n"); return;
		case 422:kgl_str_set(ret, "HTTP/1.1 422 Unprocessable Entity\r\n");return; /* WebDAV */
		case 423:kgl_str_set(ret, "HTTP/1.1 423 Locked\r\n");return; /* WebDAV */
		case 424:kgl_str_set(ret, "HTTP/1.1 424 Failed Dependency\r\n");return; /* WebDAV */
		case 426:kgl_str_set(ret, "HTTP/1.1 426 Upgrade Required\r\n");return; /* TLS */
		case 428:kgl_str_set(ret, "HTTP/1.1 428 Precondition Required\r\n"); return;
		case 429:kgl_str_set(ret, "HTTP/1.1 429 Too Many Requests\r\n"); return; 
		case 431:kgl_str_set(ret, "HTTP/1.1 431 Request Header Fields Too Large\r\n"); return;
		case 451:kgl_str_set(ret, "HTTP/1.1 451 Unavailable For Legal Reasons\r\n"); return;
		case 497:kgl_str_set(ret, "HTTP/1.1 497 Http to Https\r\n"); return;
		case 498:kgl_str_set(ret, "HTTP/1.1 498 Invalid Token\r\n"); return;
		case 499:kgl_str_set(ret, "HTTP/1.1 499 Token Required\r\n"); return;
		//5XX
		case 500:kgl_str_set(ret, "HTTP/1.1 500 Internal Server Error\r\n");return;
		case 501:kgl_str_set(ret, "HTTP/1.1 501 Not Implemented\r\n");return;
		case 502:kgl_str_set(ret, "HTTP/1.1 502 Bad Gateway\r\n");return;
		case 503:kgl_str_set(ret, "HTTP/1.1 503 Service Not Available\r\n");return;
		case 504:kgl_str_set(ret, "HTTP/1.1 504 Gateway Timeout\r\n");return;
		case 505:kgl_str_set(ret, "HTTP/1.1 505 HTTP Version Not Supported\r\n");return;
		case 507:kgl_str_set(ret, "HTTP/1.1 507 Insufficient Storage\r\n");return;
		case 508:kgl_str_set(ret, "HTTP/1.1 508 Loop Detected\r\n"); return;
		case 509:kgl_str_set(ret, "HTTP/1.1 509 Bandwidth Limit Exceeded\r\n"); return;
		case 510:kgl_str_set(ret, "HTTP/1.1 510 Not Extended\r\n"); return;
		case 511:kgl_str_set(ret, "HTTP/1.1 511 Network Authentication Required\r\n"); return;
		default: {
			ret->data = (char *)kgl_pnalloc(pool, 32);
			ret->len = snprintf(ret->data, 32, "HTTP/1.1 %d unknow\r\n", status);
			return;
			//kgl_str_set(ret, "HTTP/1.1 999 unknow\r\n"); return;
		}
	}
}
#endif /* KHTTPKEYVALUE_H_ */
