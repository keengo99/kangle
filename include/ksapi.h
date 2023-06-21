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
KBEGIN_DECLS
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
typedef VOID* LPVOID;
typedef VOID* PVOID;
typedef char                CHAR;
typedef CHAR* LPSTR;
typedef BYTE* LPBYTE;
typedef DWORD* LPDWORD;

typedef void* HANDLE;
typedef const char* LPCSTR;
typedef unsigned short      USHORT;
typedef unsigned long long  ULONGLONG;
typedef int                 HRESULT;
typedef void* HMODULE;
typedef void* HINSTANCE;
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

typedef struct _kgl_upstream kgl_upstream;
typedef struct _kgl_out_filter kgl_out_filter;
typedef struct _kgl_in_filter kgl_in_filter;

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
#define   KGL_REQ_TIMER_FIBER                      (KGL_REQ_RESERV_COMMAND+10)
#define   KGL_REQ_REGISTER_UPSTREAM                (KGL_REQ_RESERV_COMMAND+11)
#define   KGL_REQ_REGISTER_VARY                    (KGL_REQ_RESERV_COMMAND+12)
#define   KGL_REQ_MODULE_SHUTDOWN                  (KGL_REQ_RESERV_COMMAND+13)

typedef struct _kgl_command_env
{
	char* name;
	char* val;
	struct _kgl_command_env* next;
} kgl_command_env;

typedef struct _kgl_process_std
{
	PIPE_T hstdin;
	PIPE_T hstdout;
	PIPE_T hstderr;
	const char* stdin_file;
	const char* stdout_file;
	const char* stderr_file;
} kgl_process_std;
typedef struct _kgl_command
{
	const char* vh;
	const char* cmd;
	const char* dir;
	kgl_command_env* env;
	kgl_process_std std;
} kgl_command;
typedef struct _kgl_thread
{
	kev_result(*thread_function)(void* param, int msec);
	void* param;
	void* worker;
} kgl_thread;

typedef int(*kgl_fiber_start_func)(void* arg, int len);

typedef struct _kgl_timer
{
	union
	{
		result_callback cb;
		kgl_fiber_start_func fiber;
		KOPAQUE data;
	};
	void* arg;
	int msec;

} kgl_timer;
#define ASYNC_HOOK_OK          0
#define ASYNC_HOOK_ERROR       1
/* 读入post回调,返回length */
typedef int (WINAPI* http_post_hook) (void* arg, char* buf, int len);
/* 返回0成功，其他错误*/
typedef int (WINAPI* http_header_hook)(void* arg, const char* name, int name_len, const char* val, int val_len);
typedef int (WINAPI* http_header_finish_hook)(void* arg, uint16_t status_code, int64_t body_size);
/* http内容回调 data=NULL 结束,len=1表示正常结束 ,返回0成功，其他错误*/
typedef int (WINAPI* http_body_hook)(void* arg, const char* data, int len);

#define KF_SIMULATE_CACHE    1
#define KF_SIMULATE_LOCAL    4
#define KF_SIMULATE_GZIP     8


typedef struct _kgl_async_http
{
	void* arg;
	const char* host;
	const char* meth;
	const char* url;
	const char* queue;
	KHttpHeader* rh;

	http_header_hook header;
	http_header_finish_hook header_finish;	
	http_body_hook body;

	http_post_hook post;
	int64_t post_len;

	int32_t life_time;
	int32_t flags;
	int32_t port;
} kgl_async_http;



#define KF_MAX_USERNAME         (256+1)
#define KF_MAX_PASSWORD         (256+1)
#define KF_MAX_AUTH_TYPE        (32+1)

#define KF_MAX_FILTER_DESC_LEN  (256+1)


#define KF_STATUS_REQ_FINISHED  (1<<31)
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
	KF_REQ_UPSTREAM = 14,
	KF_REQ_OUT_FILTER = 15,
	KF_REQ_IN_FILTER  = 16
} KF_REQ_TYPE;

typedef enum
{
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
	KGL_VAR_HAS_CONNECTION_UPGRADE,//bool
	KGL_VAR_CONTENT_TYPE,
	KGL_VAR_CACHE_TYPE
} KGL_VAR;

typedef enum _KGL_GVAR
{
	KGL_GNAME,
	KGL_GBASE_PATH,
	KGL_GCONFIG
} KGL_GVAR;

typedef struct _kgl_output_stream_ctx kgl_output_stream_ctx;
typedef struct _kgl_input_stream_ctx kgl_input_stream_ctx;
typedef struct _kgl_response_body_ctx kgl_response_body_ctx;
typedef struct _kgl_request_body_ctx kgl_request_body_ctx;
typedef struct _kgl_parse_header_ctx kgl_parse_header_ctx;

typedef LPVOID KCONN;

typedef LPVOID KSOCKET_CLIENT;
typedef LPVOID KSOCKET_SERVER;
typedef LPVOID KSOCKET_SERVER_THREAD;
typedef LPVOID KGL_SSL_CTX;
typedef LPVOID KREQUEST;
typedef LPVOID KASYNC_FILE;
typedef LPVOID KSELECTOR;
typedef LPVOID KHTTPOBJECT;
typedef LPVOID KFIBER;
typedef LPVOID KFIBER_MUTEX;
typedef LPVOID KFIBER_COND;
typedef LPVOID KFIBER_CHAN;
typedef LPVOID KFIBER_RWLOCK;

typedef KGL_RESULT(*kgl_get_variable_f) (KREQUEST r, KGL_VAR type, const char* name, LPVOID value, LPDWORD size);

typedef enum _KF_ALLOC_MEMORY_TYPE
{
	KF_ALLOC_CONNECT = 0,
	KF_ALLOC_REQUEST = 1
} KF_ALLOC_MEMORY_TYPE;


typedef struct
{
	KGL_RESULT(*writev)(kgl_response_body_ctx* ctx, WSABUF* bufs, int bc);
	KGL_RESULT(*write)(kgl_response_body_ctx* ctx, const char* buf, int size);
	KGL_RESULT(*flush)(kgl_response_body_ctx* ctx);
	bool (*support_sendfile)(kgl_response_body_ctx* ctx);
	KGL_RESULT(*sendfile)(kgl_response_body_ctx* ctx, KASYNC_FILE fp, int64_t* length);
	KGL_RESULT(*close)(kgl_response_body_ctx* ctx, KGL_RESULT result);
} kgl_response_body_function;

typedef struct {
	kgl_response_body_function* f;
	kgl_response_body_ctx *ctx;
} kgl_response_body;

typedef struct {
	int64_t(*get_left)(kgl_request_body_ctx* ctx);/* return -1 is mean the input body unknow length, like chunk. */
	int (*read)(kgl_request_body_ctx* ctx, char* buf, int len);/* return 0 mean body is end, -1 is error */
	void (*close)(kgl_request_body_ctx* ctx);
} kgl_request_body_function;

typedef struct {
	kgl_request_body_function* f;
	kgl_request_body_ctx* ctx;
} kgl_request_body;



struct _kgl_out_filter
{
	int32_t size;
#define KGL_FILTER_NOT_CHANGE_LENGTH 1 /* do not change content length */
#define KGL_FILTER_CACHE             2 /* filter cache */
#define KGL_FILTER_NOT_CACHE         4 /* filter not cache */
	int32_t flags;
	KGL_RESULT (*tee_body)(kgl_response_body_ctx *ctx, KREQUEST rq, kgl_response_body *body);
};
struct _kgl_in_filter
{
	int32_t size;
	int32_t flags;
	KGL_RESULT(*tee_body)(kgl_request_body_ctx* ctx, KREQUEST rq, kgl_request_body* body);
};
typedef struct _kgl_access_function
{
	kgl_get_variable_f get_variable;
	KGL_RESULT(*support_function) (
		KREQUEST rq,
		KCONN cn,
		KF_REQ_TYPE	 req,
		PVOID  pData,
		PVOID* ret);
	KGL_RESULT(*response_header) (
		KREQUEST rq,
		KCONN cn,
		const char* attr,
		hlen_t attr_len,
		const char* val,
		hlen_t val_len);
	/* 在请求控制中,add_header表示增加请求头,回应控制中为空 */
	KGL_RESULT(*add_request_header) (
		KREQUEST rq,
		KCONN cn,
		const char* attr,
		hlen_t attr_len,
		const char* val,
		hlen_t val_len);
} kgl_access_function;


typedef struct _kgl_access_context
{
	KCONN cn;
	PVOID module;
	kgl_access_function* f;
}  kgl_access_context;

typedef struct _kgl_input_stream kgl_input_stream;
typedef struct _kgl_output_stream kgl_output_stream;


typedef struct _kgl_parse_header_function {
	KGL_RESULT(*parse_header)(kgl_parse_header_ctx* ctx, kgl_header_type attr, const char* val, int val_len);
	KGL_RESULT(*parse_unknow_header)(kgl_parse_header_ctx* ctx, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len);
} kgl_parse_header_function;


typedef struct _kgl_input_stream_function
{
	kgl_request_body_function body;
	kgl_url *(*get_url)(kgl_input_stream_ctx* ctx);
	kgl_precondition *(*get_precondition)(kgl_input_stream_ctx* ctx, kgl_precondition_flag* flag);
	/* return NULL if no range , if-range type return by get_precondition param flag */
	kgl_request_range *(*get_range)(kgl_input_stream_ctx* ctx);
	int (*get_header_count)(kgl_input_stream_ctx* ctx);/* do not include host,precondition,and range header */
	KGL_RESULT (*get_header)(kgl_input_stream_ctx *ctx, kgl_parse_header_ctx* cb_ctx, kgl_parse_header_function* cb);
} kgl_input_stream_function;

typedef struct _kgl_output_stream_function
{
	//out header
	void(*write_status)(kgl_output_stream_ctx* ctx, uint16_t status_code);
	KGL_RESULT(*write_header)(kgl_output_stream_ctx* ctx, kgl_header_type attr, const char* val, int val_len);
	KGL_RESULT(*write_unknow_header)(kgl_output_stream_ctx *ctx, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len);
	KGL_RESULT(*error)(kgl_output_stream_ctx *ctx, uint16_t status_code, const char* reason, size_t reason_len);
	/* if return KGL_OK body must call close to release */
	KGL_RESULT(*write_header_finish)(kgl_output_stream_ctx* ctx, int64_t body_size, kgl_response_body *body);
	KGL_RESULT(*write_trailer)(kgl_output_stream_ctx* ctx, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len);
	void (*close)(kgl_output_stream_ctx* ctx);
} kgl_output_stream_function;

struct _kgl_input_stream
{
	kgl_input_stream_function* f;
	union {
		kgl_input_stream_ctx* ctx;
		kgl_request_body_ctx* body_ctx;
	};
};
struct _kgl_output_stream
{
	kgl_output_stream_function* f;
	kgl_output_stream_ctx *ctx;
};

typedef struct _kgl_async_context_function
{

	kgl_get_variable_f get_variable;	
	KGL_RESULT(*open_next)(KREQUEST rq, KCONN cn, kgl_input_stream* in, kgl_output_stream* out, const char* queue);
	KGL_RESULT(*replace_next)(KREQUEST rq, KCONN cn, const char* us);
	KGL_RESULT(*support_function) (
		KREQUEST    rq,
		KCONN       cn,
		KF_REQ_TYPE req,
		PVOID       buffer,
		PVOID* ret);
	void (*readhup)(KREQUEST rq);
} kgl_async_context_function;

typedef struct _kgl_async_context
{
	KCONN         cn;
	PVOID         module;
	kgl_input_stream* in;
	kgl_output_stream* out;
	kgl_async_context_function* f;
} kgl_async_context;

#define kgl_get_out_async_context(x) ((kgl_async_context *)x)

typedef struct _kgl_vary_conext
{
	KCONN   cn;
	kgl_get_variable_f get_variable;
	void (*write)(KCONN cn, const char* str, int len);

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
#define KF_ACCESS_BUILD_TYPE    uint32_t
#define KF_ACCESS_BUILD_SHORT   0
#define KF_ACCESS_BUILD_HTML    1
typedef struct _kgl_access_build
{
	KCONN cn;
	void* module;
	int (*write_int)(KCONN cn, int value);
	int (*write_string) (
		KCONN cn,
		const char* str,
		int len,
		int build_flags);

} kgl_access_build;

typedef struct _kgl_config_diff
{
	//[from,to)
	uint32_t from;
	uint32_t old_to;
	uint32_t new_to;
} kgl_config_diff;

typedef struct _kgl_config_body kgl_config_body;
typedef struct _kgl_config_node kgl_config_node;

typedef struct _kgl_config_body_f
{	
	const char* (*get_value) (const kgl_config_body*cn, const char* name);
	int64_t(*get_int)(const kgl_config_body*cn, const char* name);
	const char* (*get_text)(const kgl_config_body*cn);
	void (*set_value)(kgl_config_body*cn, const char* name, const char* value);
	void (*set_int)(kgl_config_body*cn, const char* name, int64_t value);
	void (*set_text)(kgl_config_body*cn, const char* value);
	kgl_config_node*(*find_child)(const kgl_config_body*cn, const char* name);
	kgl_config_body*(*new_child)(kgl_config_body*cn, const char* name);
} kgl_config_body_f;

typedef struct _kgl_config_node_f
{
	const char* (*get_tag)(const kgl_config_node*cn);
	uint32_t(*get_count)(const kgl_config_node*cn);
	kgl_config_body *(*get_body)(const kgl_config_node*cn, uint32_t index);
	kgl_config_body *(*new_body) (kgl_config_node*cn, uint32_t index);
} kgl_config_node_f;


typedef struct _kgl_access_parse_config {
	void* module;
	const kgl_config_body* cn;
	kgl_config_body_f* body;
	kgl_config_node_f* node;
} kgl_access_parse_config;

typedef struct _kgl_access_parse_child {
	void* module;
	const kgl_config_node*o;
	const kgl_config_node*n;
	const kgl_config_diff* diff;
	kgl_config_body_f* body;
	kgl_config_node_f* node;
} kgl_access_parse_child;

typedef struct _kgl_access
{
	int32_t size;
	int32_t flags;
	const char* name;
	void* (*create_ctx)();
	void (*free_ctx)(void* ctx);
	KGL_RESULT(*build)(kgl_access_build* build_ctx, uint32_t build_type);
	KGL_RESULT(*parse_config)(kgl_access_parse_config *parse_ctx);
	KGL_RESULT(*parse_child)(kgl_access_parse_child *parse_ctx);
	uint32_t(*process)(KREQUEST rq, kgl_access_context* ctx, DWORD notify);
	void (*init_shutdown)(void* ctx, bool is_shutdown);
} kgl_access;

struct _kgl_upstream
{
	int32_t size;
	/* open upstream before cache handle */
#define KGL_UPSTREAM_BEFORE_CACHE  1
#define KGL_UPSTREAM_FINAL_SOURCE  2
	int32_t flags;
	const char* name;
	void* (*create_ctx)();
	void (*free_ctx)(void* ctx);
	KGL_RESULT(*open)(KREQUEST rq, kgl_async_context* ctx);
	void (*on_readhup)(KREQUEST rq, kgl_async_context* ctx);
};

typedef struct _kgl_http_upstream
{
	kgl_upstream* us;
	void* us_ctx;
	kgl_async_http http_ctx;
} kgl_http_upstream;


typedef struct _kgl_vary
{
	const char* name;
	bool (*build)(kgl_vary_context* ctx, const char* value);
	bool (*response)(kgl_vary_context* ctx, const char* value);
} kgl_vary;
typedef struct _kgl_socket_client_function
{
	int (*resolv)(const char* host, kgl_addr** addr);
	void (*free_addr)(kgl_addr* addr);
	SOCKET(*get_system_socket)(KSOCKET_CLIENT s);
	KSOCKET_CLIENT(*create)(const struct sockaddr* addr, socklen_t addr_len);
	KSOCKET_CLIENT(*create2)(struct addrinfo* ai, uint16_t port);
	int (*connect)(KSOCKET_CLIENT s, const char* local_ip, uint16_t local_port);
	int (*readv)(KSOCKET_CLIENT s, WSABUF* bufs, int bc);
	int (*writev)(KSOCKET_CLIENT s, WSABUF* bufs, int bc);
	bool (*read_full)(KSOCKET_CLIENT s, char* buf, int* len);
	bool (*writev_full)(KSOCKET_CLIENT s, WSABUF* bufs, int* bc);
	KSELECTOR(*get_selector)(KSOCKET_CLIENT s);
	void(*set_opaque)(KSOCKET_CLIENT s, KOPAQUE data);
	KOPAQUE(*get_opaque)(KSOCKET_CLIENT s);
	void(*shutdown)(KSOCKET_CLIENT s);
	void(*close)(KSOCKET_CLIENT s);
} kgl_socket_client_function;

typedef struct _kgl_file_function
{
	KASYNC_FILE(*open)(const char* filename, fileModel model, int kf_flags);
	KSELECTOR(*get_selector)(KASYNC_FILE fp);
	FILE_HANDLE(*get_handle)(KASYNC_FILE fp);
	void(*close)(KASYNC_FILE fp);
	int (*seek)(KASYNC_FILE fp, seekPosion pos, int64_t offset);
	int64_t(*tell)(KASYNC_FILE fp);
	int64_t(*get_size)(KASYNC_FILE fp);
	int (*read)(KASYNC_FILE fp, char* buf, int length);
	int (*write)(KASYNC_FILE fp, const char* buf, int length);
	const char* (*adjust_read_buffer)(KASYNC_FILE fp, char* buf);
	bool (*direct)(KASYNC_FILE fp,bool on_flag);
	void* (*alloc_aio_buffer)(size_t size);
	void (*free_aio_buffer)(void* ptr);
} kgl_file_function;




typedef struct _kgl_mutex_function
{
	KFIBER_MUTEX(*init)(int limit);
	void (*set_limit)(KFIBER_MUTEX mutex, int limit);
	int (*lock)(KFIBER_MUTEX mutex);
	int (*try_lock)(KFIBER_MUTEX mutex, int max);
	int (*unlock)(KFIBER_MUTEX mutex);
	void (*destroy)(KFIBER_MUTEX mutex);
} kgl_mutex_function;

typedef struct _kgl_cond_function
{
	KFIBER_COND(*init)(bool thread_safe, bool auto_reset);
	int (*wait)(KFIBER_COND mutex, int *got);
	int (*notice)(KFIBER_COND mutex, int got);
	void (*destroy)(KFIBER_COND mutex);
} kgl_cond_function;

typedef struct _kgl_chan_function {
	KFIBER_CHAN(*init)();
	int (*send)(KFIBER_CHAN ch, KOPAQUE data);
	int (*recv)(KFIBER_CHAN ch, KOPAQUE* data, KFIBER *sender);
	int (*close)(KFIBER_CHAN ch);
	void (*wakeup_sender)(KFIBER_CHAN ch, KFIBER sender, int got);
	KFIBER_CHAN(*add_ref)(KFIBER_CHAN ch);
	int (*get_ref)(KFIBER_CHAN ch);
	void (*release)(KFIBER_CHAN ch);
} kgl_chan_function;

typedef struct _kgl_kfiber_function
{
	int (*create) (kgl_fiber_start_func start, void* arg, int len, int stk_size, KFIBER* fiber);
	int (*create2) (KSELECTOR selector, kgl_fiber_start_func start, void* arg, int len, int stk_size, KFIBER* fiber);
	int (*join)(KFIBER fiber, int* retval);
	int (*msleep)(int msec);
	KFIBER(*self)();
	KFIBER(*ref_self)(bool thread_safe);
	void (*wakeup)(KFIBER fiber, void* obj, int retval);
	int (*wait)(void* obj);
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
		PVOID* ret
		);
	KGL_RESULT(*get_variable) (
		KCONN                        cn,
		KGL_GVAR                     type,
		const char* name,
		LPVOID                       value,
		LPDWORD                      size
		);
	int(*get_selector_count)();
	int(*get_selector_index)();
	KSELECTOR(*get_thread_selector)();
	KSELECTOR(*get_perfect_selector)();
	bool(*is_same_thread)(KSELECTOR selector);
	void(*next)(KSELECTOR selector, KOPAQUE data, result_callback result, void* arg, int got);
	kgl_async_context* (*get_async_context)(KCONN cn);
	void* (*alloc_memory) (KREQUEST rq, int  size, KF_ALLOC_MEMORY_TYPE type);
	KGL_RESULT(*register_clean_callback)(KREQUEST rq, kgl_cleanup_f cb, void* arg, KF_ALLOC_MEMORY_TYPE type);
	void(*log)(int level, const char* fmt, ...);
	const char* (*get_header_name)(KHttpHeader* header, hlen_t* len);
	const char* (*get_know_header)(kgl_header_type type, hlen_t* len);
	bool (*build_know_header_value)(KHttpHeader* header, const char* val, int val_len, void* (*kgl_malloc)(void*,size_t), void* arg);
	kgl_header_type(*parse_response_header)(const char* attr, hlen_t attr_len);
	kgl_header_type(*parse_request_header)(const char* attr, hlen_t attr_len);
} kgl_dso_function;


typedef struct _kgl_dso_version
{
	int32_t  size;
	int32_t  api_version; /* 双方交换api版本 */
	int32_t  module_version;
	int32_t  flags;
	KCONN    cn;
	kgl_dso_function* f;
	kgl_socket_client_function* socket_client;
	kgl_file_function* file;
	kgl_http_object_function* obj;
	kgl_kfiber_function* fiber;
	kgl_mutex_function* mutex;
	kgl_cond_function* cond;
	kgl_chan_function* chan;
} kgl_dso_version;

#define KGL_REGISTER_ACCESS(dso_version,access) dso_version->f->global_support_function(dso_version->cn,KGL_REQ_REGISTER_ACCESS,access,NULL)
#define KGL_REGISTER_UPSTREAM(dso_version,us) dso_version->f->global_support_function(dso_version->cn,KGL_REQ_REGISTER_UPSTREAM,us,NULL)
#define KGL_REGISTER_VARY(dso_version,vary) dso_version->f->global_support_function(dso_version->cn,KGL_REQ_REGISTER_VARY,vary,NULL)

DLL_PUBLIC BOOL kgl_dso_init(kgl_dso_version* ver);
DLL_PUBLIC BOOL kgl_dso_finit(int32_t flag);
KEND_DECLS
#endif /* SAPI_H_ */
