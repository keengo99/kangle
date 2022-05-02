/*
 * ksapi.h
 *
 *  Created on: 2010-6-13
 *      Author: keengo
 */

#ifndef KSAPI_H_
#define KSAPI_H_
#include "kfeature.h"
#include "kforwin32.h"
#include "khttp.h"

#define KSAPI_MAIN_VERSION 1
#define KSAPI_MIN_VERSION  0
#define KSAPI_VERSION    MAKELONG(KSAPI_MIN_VERSION, KSAPI_MAIN_VERSION)
#ifdef __cplusplus
extern "C" {
#endif
#define IS_KSAPI_VERSION_COMPATIBLE(ver) (HIWORD(ver)==KSAPI_MAIN_VERSION && LOWORD(ver)>=KSAPI_MIN_VERSION)

#ifdef _WIN32
#include <windows.h>
#else
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_NO_DATA             232 
#define ERROR_INVALID_INDEX       1413
#define WINAPI  
#define APIENTRY
#define MAX_PATH	260
#define IN
#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif
#define DLL_PROCESS_ATTACH	1
#define DLL_THREAD_ATTACH	2
#define DLL_THREAD_DETACH	3
#define DLL_PROCESS_DETACH	0
typedef unsigned int        DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef void                VOID;
typedef VOID *              LPVOID;
typedef VOID *              PVOID;
typedef char                CHAR;
typedef CHAR *              LPSTR;
typedef BYTE *              LPBYTE;
typedef DWORD *             LPDWORD;

typedef void *              HANDLE;
typedef const char *        LPCSTR;
typedef unsigned short      USHORT;
typedef unsigned long long  ULONGLONG;
typedef int                 HRESULT;
typedef void *              HMODULE;
typedef void *              HINSTANCE;
void SetLastError(DWORD errorCode);
#endif
#ifdef _WIN32
#define PIPE_T	HANDLE
#define ClosePipe	CloseHandle
#define INVALIDE_PIPE	INVALID_HANDLE_VALUE
#else
#define PIPE_T	int
#define ClosePipe	::close
#define INVALIDE_PIPE	-1
#endif
typedef struct _kgl_async_upstream kgl_async_upstream;
typedef struct _kgl_filter kgl_filter;

#define   KGL_REQ_RESERV_COMMAND                    100000
#define   KGL_REQ_COMMAND                          (KGL_REQ_RESERV_COMMAND+1)
#define   KGL_REQ_THREAD                           (KGL_REQ_RESERV_COMMAND+2)
#define   KGL_REQ_CREATE_WORKER                    (KGL_REQ_RESERV_COMMAND+3)
#define   KGL_REQ_RELEASE_WORKER                   (KGL_REQ_RESERV_COMMAND+4)
#define   KGL_REQ_SERVER_VAR                       (KGL_REQ_RESERV_COMMAND+5)
#define   KGL_REQ_TIMER                            (KGL_REQ_RESERV_COMMAND+6)
#define   KGL_REQ_ASYNC_HTTP                       (KGL_REQ_RESERV_COMMAND+7)
#define   KGL_REQ_ASYNC_HTTP_UPSTREAM              (KGL_REQ_RESERV_COMMAND+8)
#define   KGL_REQ_REGISTER_ACCESS                  (KGL_REQ_RESERV_COMMAND+9)
#define   KGL_REQ_REGISTER_SYNC_UPSTREAM           (KGL_REQ_RESERV_COMMAND+10)
#define   KGL_REQ_REGISTER_ASYNC_UPSTREAM          (KGL_REQ_RESERV_COMMAND+11)
#define   KGL_REQ_REGISTER_VARY                    (KGL_REQ_RESERV_COMMAND+12)
#define   KGL_REQ_MODULE_SHUTDOWN                  (KGL_REQ_RESERV_COMMAND+13)


typedef struct _kgl_command_env
{
	char *name;
	char *val;
	struct _kgl_command_env *next;
} kgl_command_env;

typedef struct _kgl_process_std
{	
	PIPE_T hstdin;
	PIPE_T hstdout;
	PIPE_T hstderr;
	const char *stdin_file;
	const char *stdout_file;
	const char *stderr_file;
} kgl_process_std;
typedef struct _kgl_command
{
	const char *vh;
	const char *cmd;
	const char *dir;
	kgl_command_env *env;
	kgl_process_std std;
} kgl_command;
typedef struct _kgl_thread
{
	kev_result (*thread_function)(void *param,int msec);
	void *param;
	void *worker;
} kgl_thread;
typedef struct _kgl_timer
{
	result_callback cb;
	void *arg;
	int msec;

} kgl_timer;
#define ASYNC_HOOK_OK          0
#define ASYNC_HOOK_ERROR       1
/* 读入post回调,返回length */
typedef int (WINAPI *http_post_hook) (void *arg,char *buf,int len);
/* 返回0成功，其他错误*/
typedef int (WINAPI *http_header_hook)(void *arg,int code,KHttpHeader *header);
/* http内容回调 data=NULL 结束,len=1表示正常结束 ,返回0成功，其他错误*/
typedef int (WINAPI *http_body_hook)(void *arg,const char *data,int len);

#define KF_SIMULATE_CACHE    1
#define KF_SIMULATE_LOCAL    4
#define KF_SIMULATE_GZIP     8


typedef struct _kgl_async_http
{
	void *arg;
	const char *host;
	const char *meth;
	const char *url;
	const char *queue;
	KHttpHeader *rh;
	http_header_hook header;

	http_post_hook post;	
	http_body_hook body;

	int64_t post_len;
	int32_t life_time;
	int32_t flags;
	int32_t port;
	//int32_t selector;
} kgl_async_http;


/*******************************http filter ****************************************/

#define KF_MAX_USERNAME         (256+1)
#define KF_MAX_PASSWORD         (256+1)
#define KF_MAX_AUTH_TYPE        (32+1)

#define KF_MAX_FILTER_DESC_LEN  (256+1)


#define KF_STATUS_REQ_FINISHED  (1<<31)
#define KF_STATUS_REQ_SKIP_OPEN (1<<30)
#define KF_STATUS_REQ_TRUE		1 /* check result true */
#define KF_STATUS_REQ_FALSE     0 /* check result false*/



typedef enum _KF_REQ_TYPE
{
	KD_REQ_REWRITE_URL = 1,
	KD_REQ_REWRITE_PARAM = 2,
	KD_REQ_CHECK_WHITE_LIST = 3,
	KD_REQ_ADD_WHITE_LIST = 4,
	KD_REQ_OBJ_IDENTITY = 5,
	//only support in access
	KF_REQ_UPSTREAM              = 14,
	KF_REQ_FILTER                = 15,

} KF_REQ_TYPE;

typedef enum {
	KGL_VAR_HEADER = 0,
	KGL_VAR_SSL_VAR,
	KGL_VAR_HTTPS,//int
	KGL_VAR_SERVER_PROTOCOL,
	KGL_VAR_SERVER_NAME,
	KGL_VAR_REQUEST_METHOD,
	KGL_VAR_PATH_INFO,
	KGL_VAR_REQUEST_URI,
	KGL_VAR_URL,
	KGL_VAR_SCRIPT_NAME,
	KGL_VAR_QUERY_STRING,
	KGL_VAR_SERVER_ADDR,
	KGL_VAR_SERVER_PORT,//uint16_t
	KGL_VAR_REMOTE_ADDR,
	KGL_VAR_REMOTE_PORT,//uint16_t
	KGL_VAR_PEER_ADDR,
	KGL_VAR_DOCUMENT_ROOT,
	KGL_VAR_HAS_CONTENT_LENGTH, //bool
	KGL_VAR_CONTENT_TYPE,
	KGL_VAR_IF_MODIFIED_SINCE,//time_t
	KGL_VAR_IF_NONE_MATCH,
	KGL_VAR_IF_RANGE_TIME,//time_t
	KGL_VAR_IF_RANGE_STRING,
	KGL_VAR_CACHE_TYPE
} KGL_VAR;

typedef enum _KGL_GVAR {
	KGL_GNAME,
	KGL_GBASE_PATH,
	KGL_GCONFIG
} KGL_GVAR;


typedef LPVOID KCONN;
typedef LPVOID KSOCKET_CLIENT;
typedef LPVOID KSOCKET_SERVER;
typedef LPVOID KSOCKET_SERVER_THREAD;
typedef LPVOID KSSL_CTX;
typedef LPVOID KREQUEST;
typedef LPVOID KASYNC_FILE;
typedef LPVOID KSELECTOR;
typedef LPVOID KHTTPOBJECT;
typedef LPVOID KFIBER;
typedef LPVOID KFIBER_MUTEX;
typedef LPVOID KFIBER_RWLOCK;

typedef KGL_RESULT(*kgl_get_variable_f) (KREQUEST r, KGL_VAR type, LPSTR  name, LPVOID value, LPDWORD size);

typedef enum _KF_ALLOC_MEMORY_TYPE
{
	KF_ALLOC_CONNECT = 0,
	KF_ALLOC_REQUEST = 1
} KF_ALLOC_MEMORY_TYPE;

typedef struct _kgl_filter_conext_function
{
	kgl_get_variable_f get_variable;
	KGL_RESULT (*write_all)(KREQUEST rq, KCONN cn, const char *buf, DWORD size);
	KGL_RESULT (*flush)(KREQUEST rq, KCONN cn);
	KGL_RESULT (*write_end)(KREQUEST rq, KCONN cn, KGL_RESULT result);
} kgl_filter_conext_function;

typedef struct _kgl_filter_context
{
	KCONN          cn;
	PVOID          module;
	kgl_filter_conext_function *f;
} kgl_filter_context;

struct _kgl_filter
{
	const char *name;
	KGL_RESULT (*write_all)(KREQUEST rq, kgl_filter_context *ctx, const char *buf,DWORD size);
	KGL_RESULT (*flush)(KREQUEST rq,kgl_filter_context *ctx);
	KGL_RESULT (*write_end)(KREQUEST rq, kgl_filter_context *ctx, KGL_RESULT result);
	void (*release)(void *model_ctx);
};
typedef struct _kgl_access_function
{
	kgl_get_variable_f get_variable;
	KGL_RESULT(*support_function) (
		KREQUEST rq,
		KCONN cn,
		KF_REQ_TYPE	 req,
		PVOID  pData,
		PVOID  *ret);
	KGL_RESULT(*response_header) (
		KREQUEST rq,
		KCONN cn,
		const char *attr,
		hlen_t attr_len,
		const char *val,
		hlen_t val_len);
	/* 在请求控制中,add_header表示增加请求头,回应控制中为空 */
	KGL_RESULT(*add_request_header) (
		KREQUEST rq,
		KCONN cn,
		const char *attr,
		hlen_t attr_len,
		const char *val,
		hlen_t val_len);
} kgl_access_function;


typedef struct _kgl_access_context
{
	KCONN cn;
	PVOID module;
	kgl_access_function *f;
}  kgl_access_context;

typedef struct _kgl_input_stream kgl_input_stream;
typedef struct _kgl_output_stream kgl_output_stream;

typedef enum _KGL_MSG_TYPE
{
	KGL_MSG_ERROR,
	KGL_MSG_RAW,
	KGL_MSG_VECTOR
} KGL_MSG_TYPE;

typedef struct _kgl_input_stream_function {
	int64_t(*get_read_left)(kgl_input_stream* in, KREQUEST rq);
	int (*read_body)(kgl_input_stream* in, KREQUEST rq, char* buf, int len);
	//release
	void(*release)(kgl_input_stream* in);
} kgl_input_stream_function;

typedef struct _kgl_output_stream_function {
	//out header
	void(*write_status)(kgl_output_stream *out, KREQUEST rq, int status_code);
	KGL_RESULT(*write_header)(kgl_output_stream* out, KREQUEST rq, kgl_header_type attr, const char *val, hlen_t val_len);
	KGL_RESULT(*write_unknow_header)(kgl_output_stream* out, KREQUEST rq, const char *attr, hlen_t attr_len, const char *val, hlen_t val_len);
	KGL_RESULT(*write_header_finish)(kgl_output_stream* out, KREQUEST rq);
	//out body
	KGL_RESULT(*write_body)(kgl_output_stream* out, KREQUEST rq, const char *str, int len);
	KGL_RESULT(*write_message)(kgl_output_stream* out, KREQUEST rq, KGL_MSG_TYPE msg_type, const void *msg, int msg_flag);
	KGL_RESULT(*write_end)(kgl_output_stream *out, KREQUEST rq, KGL_RESULT result);
	//release
	void(*release)(kgl_output_stream* out);

} kgl_output_stream_function;

struct _kgl_input_stream {
	kgl_input_stream_function *f;
};
struct _kgl_output_stream {
	kgl_output_stream_function* f;
};

typedef struct _kgl_async_context_function {

	kgl_get_variable_f get_variable;
	KGL_RESULT (*open_next)(KREQUEST rq, KCONN cn, kgl_input_stream *in, kgl_output_stream *out, const char *queue);
	KGL_RESULT (*redirect)(KREQUEST rq, KCONN cn, kgl_input_stream *in, kgl_output_stream *out, const char *target, const char *queue);
	KGL_RESULT (*support_function) (
		KREQUEST   rq,
		KCONN      cn,
		KF_REQ_TYPE req,
		PVOID     lpvBuffer,
		PVOID	 *ret);
} kgl_async_context_function;

typedef struct _kgl_async_context
{
	KCONN         cn;
	PVOID         module;
	kgl_input_stream *in;
	kgl_output_stream *out;
	kgl_async_context_function *f;
} kgl_async_context;

typedef struct _kgl_dso_input_stream {
	kgl_input_stream base;
	kgl_async_context *ctx;
} kgl_dso_input_stream;

typedef struct _kgl_dso_output_stream {
	kgl_output_stream base;
	kgl_async_context *ctx;
} kgl_dso_output_stream;

#define get_async_context kgl_get_out_async_context
#define kgl_get_in_async_context(gate)  (((kgl_dso_input_stream *)gate)->ctx)
#define kgl_get_out_async_context(gate)  (((kgl_dso_output_stream *)gate)->ctx)

typedef struct _kgl_vary_conext
{
	KCONN   cn;
	kgl_get_variable_f get_variable;
	void (*write)(KCONN cn, const char *str, int len);

} kgl_vary_context;

#define KF_NOTIFY_READ_DATA                 0x00008000
#define KF_NOTIFY_REQUEST                   0x00004000
#define KF_NOTIFY_URL_MAP                   0x00001000
#define KF_NOTIFY_RESPONSE                  0x00000040
#define KF_NOTIFY_SEND_DATA                 0x00000400
#define KF_NOTIFY_END_REQUEST               0x00000080
#define KF_NOTIFY_END_CONNECT               0x00000100


#define KF_NOTIFY_REQUEST_MARK            0x00100000
#define KF_NOTIFY_RESPONSE_MARK           0x00200000
#define KF_NOTIFY_REQUEST_ACL             0x00400000
#define KF_NOTIFY_RESPONSE_ACL            0x00800000
#define KF_NOTIFY_REPLACE                 0x01000000

#define KGL_BUILD_HTML_ENCODE   1
typedef struct _kgl_access_build
{
	KCONN cn;
	void *module;
	int (* write_string) (
		KCONN cn,
		const char *str,
		int len,
		int build_flags);
} kgl_access_build;

typedef struct _kgl_access_parse
{
	KCONN cn;
	void *module;
	const char *(*get_value) (KCONN cn,const char *name);
	int64_t (*get_int)(KCONN cn, const char *name);
} kgl_access_parse;

typedef enum _KF_ACCESS_BUILD_TYPE
{
	KF_ACCESS_BUILD_HTML = 0,
	KF_ACCESS_BUILD_SHORT,
	KF_ACCESS_BUILD_XML,
	KF_ACCESS_BUILD_XML_CHARACTER,
} KF_ACCESS_BUILD_TYPE;

typedef enum _KF_ACCESS_PARSE_TYPE
{
	KF_ACCESS_PARSE_KV = 0,
	KF_ACCESS_PARSE_XML_CHARACTER,
	KF_ACCESS_PARSE_END
} KF_ACCESS_PARSE_TYPE;

typedef struct _kgl_access
{
	int32_t size;
	int32_t flags;
	const char *name;	
	void *(*create_ctx)();
	void (*free_ctx)(void *ctx);
	KGL_RESULT (*build)(kgl_access_build *build_ctx,KF_ACCESS_BUILD_TYPE build_type);
	KGL_RESULT (*parse)(kgl_access_parse *parse_ctx, KF_ACCESS_PARSE_TYPE parse_type);
	uint32_t (*process)(KREQUEST rq, kgl_access_context *ctx,DWORD notify);
	void (*init_shutdown)(void *ctx,bool is_shutdown);
} kgl_access;

#define KF_UPSTREAM_SYNC         1

typedef struct {
	int32_t size;
	int32_t flags;
	const char *name;	
} kgl_upstream;

typedef struct _kgl_sync_upstream
{
	int32_t size;
	int32_t flags;
	const char *name;
	void *(*create_ctx)();
	void (*free_ctx)(void *ctx);
	KGL_RESULT (*process)(kgl_access_context *ctx);
} kgl_sync_upstream;

struct _kgl_async_upstream
{
	int32_t size;
	int32_t flags;
	const char *name;
	void *(*create_ctx)();
	void (*free_ctx)(void *ctx);
	uint32_t (*check)(KREQUEST rq, kgl_async_context *ctx);
	KGL_RESULT (*open)(KREQUEST rq, kgl_async_context *ctx);
};
typedef struct _kgl_async_http_upstream
{
	kgl_upstream *us;
	void *us_ctx;
	kgl_async_http http_ctx;
} kgl_async_http_upstream;

typedef struct _kgl_vary
{
	const char *name;
	bool (*build)(kgl_vary_context *ctx,const char *value);
	bool (*response)(kgl_vary_context *ctx, const char *value);
} kgl_vary;
typedef struct _kgl_socket_client_function
{
	int (*resolv)(const char *host, kgl_addr **addr);
	void (*free_addr)(kgl_addr* addr);
	SOCKET (*get_system_socket)(KSOCKET_CLIENT s);
	KSOCKET_CLIENT(*connect)(const struct sockaddr* addr, socklen_t addr_len);
	int (*writev)(KSOCKET_CLIENT s, WSABUF* buf, int bc);
	int (*readv)(KSOCKET_CLIENT s, WSABUF* buf, int bc);
	KSELECTOR(*get_selector)(KSOCKET_CLIENT s);
	void(*set_opaque)(KSOCKET_CLIENT s, KOPAQUE data);
	void(*close_client)(KSOCKET_CLIENT s);
	void(*shutdown)(KSOCKET_CLIENT s);	
} kgl_socket_client_function;

typedef struct _kgl_file_function
{
	KASYNC_FILE(*open)(const char *filename, fileModel model,int kf_flags);
	KSELECTOR(*get_selector)(KASYNC_FILE fp);
	void(*close)(KASYNC_FILE fp);
	int (*seek)(KASYNC_FILE fp,seekPosion pos, int64_t offset);
	int64_t (*tell)(KASYNC_FILE fp);
	int64_t (*get_size)(KASYNC_FILE fp);	
	int (*read)(KASYNC_FILE fp, char* buf, int length);
	int (*write)(KASYNC_FILE fp, const char* buf, int length);
	const char* (*adjust_read_buffer)(KASYNC_FILE fp, char* buf);
} kgl_file_function;


typedef int(*kgl_fiber_start_func)(void* arg, int len);

typedef struct _kgl_kfiber_function
{
	int (*create) (kgl_fiber_start_func start, void* arg, int len, int stk_size, KFIBER* fiber);
	int (*create2) (KSELECTOR selector, kgl_fiber_start_func start, void* arg, int len, int stk_size, KFIBER* fiber);
	int (*join)(KFIBER fiber, int* retval);
	int (*msleep)(int msec);
	KFIBER (*self)();
	KFIBER (*ref_self)(bool thread_safe);
	void (*wakeup)(KFIBER fiber, void* obj, int retval);
	int (*wait)(void* obj);

	KFIBER_MUTEX(*mutex_init)(int limit);
	void (*mutex_set_limit)(KFIBER_MUTEX mutex, int limit);
	int (*mutex_lock)(KFIBER_MUTEX mutex);
	int (*mutex_try_lock)(KFIBER_MUTEX mutex,int max);
	int (*mutex_unlock)(KFIBER_MUTEX mutex);
	void (*mutex_destroy)(KFIBER_MUTEX mutex);

} kgl_kfiber_function;

typedef struct _kgl_http_object_function
{
	KGL_RESULT(*get_header)(KHTTPOBJECT obj, LPSTR  name, LPVOID value, LPDWORD size);
	KHTTPOBJECT(*get_old_obj)(KREQUEST rq);
} kgl_http_object_function;

typedef struct _kgl_dso_function
{
	KGL_RESULT(*global_support_function) (
		KCONN                        cn,
		DWORD                        req,
		PVOID                        data,
		PVOID                        *ret
		);
	KGL_RESULT(*get_variable) (
		KCONN                        cn,
		KGL_GVAR                     type,
		LPSTR                        name,
		LPVOID                       value,
		LPDWORD                      size
		);
	int(*get_selector_count)();
	int(*get_selector_index)();
	KSELECTOR (*get_thread_selector)();
	KSELECTOR(*get_perfect_selector)();
	bool(*is_same_thread)(KSELECTOR selector);
	void(*next)(KSELECTOR selector, KOPAQUE data, result_callback result, void *arg, int got);
	kgl_async_context *(*get_async_context)(KCONN cn);	
	void * (*alloc_memory) (
		KREQUEST rq,
		DWORD  cbSize,
		KF_ALLOC_MEMORY_TYPE type);
	KGL_RESULT (*register_clean_callback)(KREQUEST rq, kgl_cleanup_f cb, void *arg, KF_ALLOC_MEMORY_TYPE type);	
	void(*log)(int level, const char *fmt, ...);
} kgl_dso_function;


typedef struct _kgl_dso_version
{
	int32_t  size;
	int32_t  api_version; //双方交换api版本
	int32_t  module_version;
	int32_t  flags;
	KCONN    cn;
	kgl_dso_function *f;
	kgl_socket_client_function *socket_client;
	kgl_file_function *file;
	kgl_http_object_function *obj;
	kgl_kfiber_function* fiber;
} kgl_dso_version;

#define KGL_REGISTER_ACCESS(dso_version,access) dso_version->f->global_support_function(dso_version->cn,KGL_REQ_REGISTER_ACCESS,access,NULL)
#define KGL_REGISTER_SYNC_UPSTREAM(dso_version,us) dso_version->f->global_support_function(dso_version->cn,KGL_REQ_REGISTER_SYNC_UPSTREAM,us,NULL)
#define KGL_REGISTER_ASYNC_UPSTREAM(dso_version,us) dso_version->f->global_support_function(dso_version->cn,KGL_REQ_REGISTER_ASYNC_UPSTREAM,us,NULL)
#define KGL_REGISTER_VARY(dso_version,vary) dso_version->f->global_support_function(dso_version->cn,KGL_REQ_REGISTER_VARY,vary,NULL)

DLL_PUBLIC BOOL kgl_dso_init(kgl_dso_version * ver);
DLL_PUBLIC BOOL kgl_dso_finit(DWORD flag);

static INLINE bool is_absolute_path(const char *str) {
	if (str[0] == '/') {
		return true;
	}
#ifdef _WIN32
	if (str[0] == '\\') {
		return true;
	}
	if (strlen(str) > 1 && str[1] == ':') {
		return true;
	}
#endif
	return false;
}
#ifdef __cplusplus
 }
#endif
#endif /* SAPI_H_ */
