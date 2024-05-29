# dso模块开发
dso模块是以动态链接库的形式动态加载进kangle主进程，提供服务。unix系统是so文件,windows是dll文件。

## testdso例子
kangle提供一个testdso的例子，供参考。

## ksapi.h
dso模块需包含ksapi.h文件(在kangle的include目录下).

## kgl_dso_init/kgl_dso_finit
```
BOOL kgl_dso_init(kgl_dso_version * ver);
BOOL kgl_dso_finit(int32_t flag);
```
dso模块需暴露`kgl_dso_init`和`kgl_dso_finit`两个函数。kangle加载后，调用`kgl_dso_init`，卸载时调用`kgl_dso_finit`。
成功返回`TRUE`,失败返回`FALSE`.

### kgl_dso_version
```
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
	kgl_pool_function* pool;
} kgl_dso_version;
```
* `size`指示 `kgl_dso_version`结构体大小.
* `api_version` 指示当前版本。
* `module_version` 由dso设置并返回,标识dso的版本。
* `flags` 目前没有用
* `cn` 由kangle提供，调用一些函数时回传。
* 其中`f` `socket_client` `file` `obj` `fiber` 等指向不同的函数结构,提供不同的功能。
具体可参考`ksapi.h`里面的定义。
#### kgl_dso_function
```
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
```