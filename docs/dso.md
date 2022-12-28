# dso模块开发
dso模块是以动态链接库的形式动态加载进kangle主进程，提供服务。unix系统是so文件,windows是dll文件。

## testdso例子
kangle提供一个testdso的例子，供参考。

## ksapi.h
dso模块需包含ksapi.h文件(在kangle的include目录下).

## kgl_dso_init/kgl_dso_finit
```
DLL_PUBLIC BOOL kgl_dso_init(kgl_dso_version * ver);
DLL_PUBLIC BOOL kgl_dso_finit(DWORD flag);
```
dso模块需暴露`kgl_dso_init`和`kgl_dso_finit`两个函数。kangle加载后，调用`kgl_dso_init`，卸载时调用`kgl_dso_finit`。

### kgl_dso_version
```
typedef struct _kgl_dso_version
{
	int32_t  size;
	int32_t  api_version;
	int32_t  module_version;
	int32_t  flags;
	KCONN    cn;
	kgl_dso_function *f;
	kgl_socket_client_function *socket_client;
	kgl_file_function *file;
	kgl_http_object_function *obj;
	kgl_kfiber_function* fiber;
} kgl_dso_version;
```
* `size`指示 `kgl_dso_version`结构体大小.
* `api_version` 指示当前版本。
* `module_version` 由dso设置并返回,标识dso的版本。
* `flags` 目前没有用
* `cn` 由kangle提供，调用一些函数时回传。
* 其中`f` `socket_client` `file` `obj` `fiber` 指向不同的函数结构。具体可参考`ksapi.h`里面的定义。
