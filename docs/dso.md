# DSO 扩展开发

DSO 扩展是由 kangle 主进程加载的动态库：Unix/Linux 使用 `.so`，Windows 使用 `.dll`。扩展与主进程共享地址空间，崩溃、越界访问或 ABI 不匹配会直接影响服务器稳定性，因此应使用与目标 kangle 相同的头文件、编译器 ABI 和主要编译选项构建。

## 可运行示例

当前仓库的 `module/testdso` 是完整示例，演示了：

- DSO 初始化和版本协商；
- 注册自定义 ACL/Mark；
- 注册同步、异步上游；
- 请求/响应过滤、WebSocket、sendfile 和异步回调。

主工程默认会生成 `testdso` 模块（代理专用构建除外）。推荐先构建整个项目，再从该模块裁剪自己的实现：

```bash
mkdir cmake-build
cd cmake-build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target testdso -j
```

不要只复制本文档中的结构体定义；`include/ksapi.h` 是当前 ABI 的唯一准确信息。

## 加载配置

```xml
<dso_extend name='myext' filename='bin/myext.${dso}' option='value'/>
```

- `name`：扩展的唯一名称。
- `filename`：动态库路径。`${dso}` 会按平台展开为动态库后缀。
- 其余属性会保留在扩展配置中，可由扩展按自身约定读取。

扩展注册的上游处理器可在映射中引用：

```xml
<map path='/service' extend='dso:myext:handler'
     confirm_file='0' allow_method='*'/>
```

扩展注册的访问控制模块则与内置模块一样使用：

```xml
<acl module='my_acl' option='value'/>
<mark module='my_mark' option='value'/>
```

## 必须导出的入口

扩展必须包含 `include/ksapi.h` 并以 C linkage 导出以下函数：

```c
DLL_PUBLIC BOOL kgl_dso_init(kgl_dso_version *ver);
DLL_PUBLIC BOOL kgl_dso_finit(int32_t flag);
```

kangle 加载动态库后调用 `kgl_dso_init`，卸载时调用 `kgl_dso_finit`。成功返回 `TRUE`，失败返回 `FALSE`。

最小初始化框架：

```cpp
#include "ksapi.h"

static kgl_dso_version *api = nullptr;

DLL_PUBLIC BOOL kgl_dso_init(kgl_dso_version *ver)
{
    if (ver == nullptr || !IS_KSAPI_VERSION_COMPATIBLE(ver->api_version)) {
        return FALSE;
    }

    api = ver;
    ver->api_version = KSAPI_VERSION;
    ver->module_version = MAKELONG(0, 1);

    /* 在这里使用 KGL_REGISTER_ACCESS、KGL_REGISTER_UPSTREAM 等注册接口。 */
    return TRUE;
}

DLL_PUBLIC BOOL kgl_dso_finit(int32_t flag)
{
    api = nullptr;
    return TRUE;
}
```

初始化时必须先检查 `IS_KSAPI_VERSION_COMPATIBLE`，再把 `ver->api_version` 设置为扩展实际使用的 `KSAPI_VERSION`。kangle 会在初始化返回后再次检查兼容性。

## `kgl_dso_version`

`kgl_dso_version` 由主进程填充并传给扩展，当前主要成员包括：

| 成员 | 说明 |
| --- | --- |
| `api_version` | 主进程和扩展协商的 KSAPI 版本。 |
| `module_version` | 扩展填写的自身版本。 |
| `flags` | 保留标志。 |
| `cn` | 主进程提供的扩展上下文，调用全局函数时回传。 |
| `f` | 全局函数和注册入口。 |
| `socket_client` | 异步客户端 socket 接口。 |
| `file` | 异步文件接口。 |
| `obj` | HTTP 对象接口。 |
| `fiber` | 协程接口。 |
| `mutex`、`cond`、`chan` | 同步原语接口。 |
| `pool` | 连接和请求内存池接口。 |

函数表会随 KSAPI 演进，调用前应以当前 `ksapi.h` 中的类型签名为准，不要使用旧文档或旧二进制对应的函数指针声明。

## 注册 ACL/Mark

使用 `kgl_access` 描述访问控制模块，再通过：

```cpp
KGL_REGISTER_ACCESS(ver, &access_model);
```

注册。`kgl_access::notify` 决定它作为请求 ACL、请求 Mark、响应 ACL 或响应 Mark 被调用；创建、释放、解析配置和处理回调必须与该阶段相匹配。完整且持续测试的写法见 `module/testdso/access.cpp`。

回调返回值通常使用 `KF_STATUS_REQ_TRUE`、`KF_STATUS_REQ_FALSE`；只有在扩展已经生成最终响应或明确终止处理时才返回带 `KF_STATUS_REQ_FINISHED` 的结果。

## 注册上游

全局上游使用：

```cpp
KGL_REGISTER_UPSTREAM(ver, &upstream_model);
```

也可以在某次请求中通过访问上下文注册临时异步上游。异步实现必须遵守以下规则：

- 回调只完成一次；
- 保证上下文活到最终回调；
- 在取消、连接关闭和部分读写时释放全部资源；
- 不在错误线程直接操作只能由 selector 线程访问的请求对象；
- 明确区分 EOF、暂时不可读和硬错误。

参考 `module/testdso/async_upstream.cpp` 和 `module/testdso/sync_upstream.cpp`。

## 内存与线程安全

- 请求期临时数据优先用 `ver->pool` 或 `alloc_memory` 分配，并注册清理回调。
- 不要跨请求保存请求池指针、Header 指针或 `KREQUEST`。
- DSO 默认可能被多个工作线程并发调用；全局可变状态必须加锁，或按 selector 分片。
- 使用 kangle 提供的异步、fiber、mutex 和 channel 函数表，可减少运行库和调度模型不一致的问题。
- `kgl_dso_finit` 前应停止后台任务，之后不得再调用主进程函数表。

## 版本与部署

1. 使用目标版本仓库中的 `include/ksapi.h` 编译。
2. 在测试环境加载扩展并检查错误日志；缺少 `kgl_dso_init`、版本不兼容或初始化返回失败都会导致加载失败。
3. 对扩展覆盖的请求路径做并发、连接中断、重载和关闭测试。
4. 替换动态库后重启 kangle。当前 DSO/API 的热重载能力有限，不应假定旧代码已完全卸载。

相关配置见 [config.md](./config.md)，ACL/Mark 使用方式见 [acl_mark.md](./acl_mark.md)。
