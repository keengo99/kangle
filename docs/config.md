# kangle 配置参考

本文档对应当前仓库中的配置解析器。kangle 使用 XML 配置，主配置文件为 `etc/config.xml`，还会加载 `ext` 目录中的扩展配置文件。所有配置文件都必须以 `<config>` 为根元素。

> 部分配置和模块受编译选项控制。可用 `bin/kangle -v` 查看当前二进制启用的主要功能；某个条件编译功能未启用时，相应配置不会生效。

## 基本约定

- 布尔值通常使用 `0`/`1`，少数虚拟主机属性也接受 `off`/`on`。
- 大小值可使用 `K`、`M`、`G`、`T` 后缀，例如 `256K`、`1G`。
- 相对路径通常相对于 kangle 安装目录；虚拟主机证书的普通相对路径相对于该主机的 `doc_root`。证书路径以 `-` 开头时，去掉 `-` 后相对于 kangle 安装目录。
- 修改配置后可执行 `bin/kangle -r` 平滑重载。`worker_thread`、已经初始化的磁盘缓存目录等启动期设置需要重启才能完全应用。
- 重载时如果某个配置文件读不出来（XML 语法错误、读取失败、虚拟主机数据库连不上），该文件对应的配置保持原样继续运行，并在错误日志记录 `cann't load config file ... keep its running config`；修正后再次重载即可生效。被删除或以 `<!--#stop-->` 开头的文件，其配置仍会被移除。

### 多文件加载顺序

扩展配置文件可在第一行指定加载顺序：

```xml
<!--#start 20 -->
<config>
    ...
</config>
```

数字越小越先加载，主配置文件的默认顺序为 `100`。多个文件中的同名对象（例如带相同 `name` 的 `server` 或 `vh`）会按配置树规则合并或覆盖，因此建议给扩展文件分配明确且不重复的顺序。

### 旧版 XML 配置迁移

可在源码目录使用 `tools/migrate-config.sh` 将旧版主配置以及站点 `access.xml` 转换为当前格式。默认处理 `/vhs/kangle/etc/config.xml`， `/home/ftp/*/*/access.xml`：

```sh
# 先预览，不写文件
./tools/migrate-config.sh --dry-run

# 转换、为每个变更文件创建带时间戳的备份，并平滑重载
./tools/migrate-config.sh --reload
```

脚本会把旧的 `acl_xxx`/`mark_xxx` 节点、根级超时/线程/防火墙配置和旧版错误页属性转换为新格式，移除已经内置的 Filter DSO 加载项，并补充 WebDAV、WHM 及当前资源配置。写入前会先解析全部目标文件；任何一个文件无效时不会修改其他文件。可用 `--config`、`--ftp-root`、`--access-file` 和 `--output-dir` 在副本上测试或只转换指定文件。

## 最小配置

```xml
<config>
    <listen ip='*' port='80' type='http'/>
    <listen ip='127.0.0.1' port='3311' type='manage'/>
    <timeout rw='60' connect='20'/>
    <admin user='admin' password='change-me' crypt='plain'
           auth_type='Basic' admin_ips='127.0.0.1'/>
    <request action='vhs'/>
    <response action='allow'/>
    <vhs>
        <index file='index.html'/>
        <mime_type ext='html' type='text/html' compress='1'/>
    </vhs>
    <vh name='default' doc_root='www' inherit='1'>
        <host>*</host>
    </vh>
</config>
```

## 根级基础配置

### `worker_thread`

工作线程数，以文本内容设置。`0` 或负数表示按 CPU 数量自动选择。配置值不必预先是 2 的幂，但 selector 管理器会向下取为不大于该值的 2 的幂，并最多使用 `64`（例如 `6` 实际为 `4`）；为避免歧义，建议直接配置 `1`、`2`、`4`、`8` 等值。该项只在主配置首次加载时扩展线程池，修改后应重启。

```xml
<worker_thread>4</worker_thread>
```

### `timeout`

- `rw`：客户端读写超时，单位秒，默认 `60`。
- `connect`：上游连接超时，单位秒。省略时内部值为 `0`，selector 会按 `rw` 处理；建议仍显式填写，便于阅读。

旧格式 `<timeout>60</timeout>` 和 `<connect_timeout>20</connect_timeout>` 仍可解析，但新配置应使用属性格式。

### 其他根级标量

| 元素 | 说明 |
| --- | --- |
| `lang` | 管理界面语言。 |
| `auth_delay` | 认证失败后的延迟时间，单位秒。 |
| `access_log` | 全局访问日志路径；也可使用 `<log access='...'>`，根级元素优先。 |
| `access_log_handle` | 访问日志轮转后调用的外部处理命令。 |
| `log_handle_concurrent` | 日志处理器最大并发数。 |
| `log_event_id` | 日志事件编号。 |
| `server_software` | 覆盖响应中的服务器软件标识。 |
| `cookie_stick_name` | 多上游节点 Cookie 黏着所使用的 Cookie 名。 |
| `hostname` | 本机主机名覆盖值。 |
| `http2https_code` | HTTP 跳转 HTTPS 时使用的状态码。 |
| `max_connect_info` | 保留的连接信息数量上限。 |
| `unix_socket` | Unix socket 相关开关，仅 Unix socket 构建可用。 |
| `path_info` | 是否启用 PATH_INFO 解析，`0`/`1`。 |
| `min_free_thread` | 最少空闲线程数。 |
| `upstream_sign` | 写入上游请求的签名字符串。 |
| `read_hup` | 是否响应 HUP 触发的重载，`0`/`1`。 |
| `max_post_size` | 长度未知的请求体（chunked 或未带 Content-Length 的 HTTP/2/3 上传）转发给 FastCGI（如 PHP）前会先写入临时文件，此项限制其大小，超过返回 413；`0` 表示不限制，默认 8M，支持大小单位。带 Content-Length 的请求体直接转发，不受此项限制。 |
| `flush_flow_time` | 流量统计刷新周期；仅流量统计构建可用。 |
| `process_cpu_usage` | 进程 CPU 使用限制；仅相应构建可用。 |
| `log_drill` | 日志 drill 数值，解析范围为 `0`～`65536`；仅相应构建可用。 |
| `cdnbest` | CDN 回源辅助设置，当前使用 `error` 属性指定错误 URL。 |

旧版样例中出现的 `attack`、`charset`、`insert_via`、`x_forwarded_for` 等根级元素不属于当前通用配置解析器；相关行为应通过 ACL/Mark（例如 `flag`）配置，不应继续照搬旧配置。

## 监听器 `listen`

| 属性 | 说明 |
| --- | --- |
| `ip` | 监听地址，例如 `*`、`0.0.0.0`、`::`、`127.0.0.1`。 |
| `port` | 监听端口。 |
| `type` | `http`、`https`、`manage`、`manages`；四层转发构建还可使用 `tcp`/`portmap`，代理构建可使用 `tcps`。 |

`manage` 和 `manages` 是管理控制台监听器，不建议暴露到公网。

### TLS 属性

以下属性用于 `https`、`manages` 等 TLS 监听器：

| 属性 | 说明 |
| --- | --- |
| `certificate` | PEM 证书链文件。 |
| `certificate_key` | PEM 私钥文件；省略时由 TLS 库按证书配置处理。 |
| `cipher` | OpenSSL cipher list。 |
| `protocols` | 允许的 TLS 协议列表。 |
| `alpn` | ALPN 位掩码；一般应使用下面更直观的 `http2`/`http3`。 |
| `http2` | 启用 HTTP/2，`0`/`1`。为兼容旧配置保留。 |
| `http3` | 启用 HTTP/3，`0`/`1`；仅 HTTP/3 构建可用，还需要对应 UDP/QUIC 运行环境。 |
| `early_data` | 启用 TLS early data，`0`/`1`。 |
| `reject_nosni` | 拒绝不带 SNI 的 TLS 请求，`0`/`1`；只对监听器有效。 |

```xml
<listen ip='*' port='443' type='https'
        certificate='etc/server.crt' certificate_key='etc/server.key'
        http2='1' reject_nosni='1'/>
```

启用子虚拟主机证书功能的构建还可在根级按 SNI 域名注册证书：

```xml
<ssl domain='example.com' certificate='etc/example.crt'
     certificate_key='etc/example.key'/>
```

`ssl` 支持 `domain`、`certificate` 和 `certificate_key`；同一 `domain` 的后续配置会更新该证书映射。

## 运行和资源配置

### `cache`

kangle 支持内存缓存、可选磁盘缓存和大对象分片缓存。

| 属性 | 说明 |
| --- | --- |
| `default` | 默认是否缓存，`0`/`1`，默认 `1`。 |
| `memory` | 内存缓存上限，默认 `1G`。 |
| `disk` | 磁盘缓存上限；可使用大小或百分比（如 `10%`），仅磁盘缓存构建可用。 |
| `disk_dir` | 磁盘缓存目录；空值使用安装目录下的 `cache`。首次初始化后修改通常需要重启。 |
| `refresh_time` | 默认缓存刷新时间，单位秒，默认 `30`。 |
| `max_cache_size` | 单个普通缓存对象上限，默认 `10M`；超过后可转入分片缓存。 |
| `max_bigobj_size` | 可分片缓存的大对象上限，默认 `1G`。 |
| `cache_part` | 是否允许分片缓存，`0`/`1`，默认 `1`。 |
| `disk_work_time` | 磁盘缓存清理时段，使用项目的 cron 时间格式。 |

### `compress`

| 属性 | 说明 |
| --- | --- |
| `only_cache` | 仅压缩可缓存响应，`0`/`1`，默认 `0`。 |
| `min_length` | 已知长度响应的最小压缩大小，默认 `512`。 |
| `gzip_level` | gzip 级别，默认 `5`，`0` 禁用。 |
| `br_level` | Brotli 级别，默认 `5`，`0` 禁用；需启用 Brotli。 |
| `zstd_level` | Zstandard 级别，默认 `5`，`0` 禁用；需启用 Zstandard。 |

是否允许压缩还会受到 `mime_type@compress`、客户端 `Accept-Encoding` 和响应状态等条件影响。

### `connect`

| 属性 | 说明 |
| --- | --- |
| `max` | 全局最大连接数，`0` 表示不限制。 |
| `max_per_ip` | 每个客户端 IP 的最大连接数，`0` 表示不限制。 |
| `max_keep_alive` | 最大 keep-alive 连接数，超过后改用短连接。 |
| `per_ip_deny` | 超过 `max_per_ip` 时是否加入拒绝列表，`0`/`1`。 |

达到 `max`，或进程的文件描述符用完时，kangle 会先关闭一部分正在等待下一个请求的 HTTP/1 keep-alive 连接（最久没活动的优先，每次约八分之一，最多 32 个），而不是直接拒绝新连接。正在处理请求的连接和 HTTP/2 连接不受影响。

### `fiber`、`dns`、`io`

```xml
<fiber stack_size='128K'/>
<dns worker='8'/>
<io worker='2' max='0' buffer='256K'/>
```

- `fiber@stack_size`：协程栈大小，按 4096 字节对齐。
- `dns@worker`：DNS 解析工作线程数，默认 `8`。
- `io@worker`：异步文件 I/O 工作者数，默认 `2`。
- `io@max`：最大并发文件 I/O 数，`0` 表示不限制。
- `io@buffer`：单次文件 I/O 缓冲区大小，默认 `256K`，按 1024 字节对齐。这里是字节大小，不是“buffer 数量”。

### `run_as`

```xml
<run_as user='www-data' group='www-data'/>
```

设置主进程降权后的用户和组。需要由具有切换身份权限的账户启动。旧样例中的 `<run user='...'>` 不等同于当前解析器的 `<run_as>`。

### `ssl_client`

控制 kangle 连接 HTTPS 上游时的客户端 TLS 设置：

- `ca_path`：CA 文件或目录配置。
- `chiper`：上游 cipher list。
- `protocols`：允许的 TLS 协议。

> `chiper` 是当前源码实际识别的历史拼写，不能写成 `cipher`；监听器和虚拟主机 TLS 配置则使用正确拼写 `cipher`。

### `admin`

| 属性 | 说明 |
| --- | --- |
| `user` | 管理员用户名。 |
| `password` | 密码或密码摘要，含义由 `crypt` 决定。 |
| `crypt` | 密码存储类型：`plain`、`md5`、`smd5`、`sign`，以及启用对应功能时的 `htpasswd`。未知值回退为 `plain`。 |
| `auth_type` | HTTP 认证类型，`Basic` 或 `Digest`。 |
| `admin_ips` | 允许访问管理端口的精确 IP 列表，以 `|` 分隔；`*` 匹配任意 IP。以 `~` 开头的 IP（如 `~127.0.0.1`）命中后会直接绕过 HTTP 密码认证，只应给完全可信的本机/管理地址使用。 |

不要在生产配置中保留示例密码，也不要用 `*` 放开管理来源。

浏览器标记为跨站的请求（`Sec-Fetch-Site: cross-site`，旧浏览器则看 `Origin`/`Referer` 的主机名）会被管理端口拒绝，防止其他网站借用浏览器缓存的管理员凭据执行管理操作。唯一例外是对首页 `/` 的 GET（只是框架页），以便面板中的“进入管理后台”链接可用。脚本和 API 客户端不发送这些头，不受影响。

### `log`

| 属性 | 说明 |
| --- | --- |
| `access` | 全局访问日志路径；仅在根级 `access_log` 未设置时采用。 |
| `level` | 日志级别，数值越大越详细，默认 `2`。 |
| `rotate_time` | 访问日志轮转时间，使用项目的 cron 时间格式。 |
| `rotate_size` | 单个访问日志轮转大小，默认 `100M`。 |
| `logs_day` | 日志保留天数。 |
| `logs_size` | 日志总大小上限，默认 `1G`。 |
| `error_rotate_size` | 错误日志轮转大小，默认 `100M`。 |
| `radio` | 日志采样比，例如 `3` 表示约每 3 条记录 1 条。属性名是历史拼写。 |
| `log_handle` | 是否启用日志处理器，`0`/`1`。 |

### `firewall`（条件功能）

启用黑名单功能的构建可使用：`bl_time`、`wl_time`、`block_ip_cmd`、`unblock_ip_cmd`、`flush_ip_cmd` 和 `report_url`。这些命令会由服务器进程执行，务必使用固定路径和可信参数。

## 上游服务 `server`

`server` 同时表示单节点和多节点上游。带 `host` 属性时为单节点；没有 `host`、包含 `<node>` 子元素时为多节点。

支持的协议值：`http`、`fastcgi`（别名 `fcgi`）、`ajp`、`tcp`；启用 Proxy Protocol 的构建还支持 `proxy`。未识别的值会回退为 `http`，因此不要依赖拼写错误被拒绝。旧源码中遗留的 `uwsgi`、`scgi`、`hmux` 分支当前没有启用。

### 单节点

```xml
<server name='php' proto='fastcgi' host='127.0.0.1'
        port='9000' life_time='30'/>
```

| 属性 | 说明 |
| --- | --- |
| `name` | 唯一服务名。 |
| `proto` | 上游协议；兼容旧属性名 `type`。 |
| `host` | 主机名、IPv4/IPv6 地址；Unix 构建也可使用 `unix:/path.sock` 或绝对 socket 路径。 |
| `port` | 独立端口字段，不是 `host:port`。HTTP 上游后缀见下文。 |
| `life_time` | 空闲连接复用时间，秒；`0` 表示不复用。不写时 `http` 上游默认 60 秒，其他协议默认不复用。多节点的 `<node>` 同样适用。 |
| `param` | 传给上游的附加参数（需相应构建功能）。 |
| `self_ip` | 发起上游连接时绑定的本地 IP。 |
| `sign` | 设置上游签名标志，`0`/`1`。 |
| `auth_user`、`auth_passwd` | 代理构建中的上游 Basic 认证。 |

HTTP 上游的 `port` 可在数字后追加：

- `s`：TLS；
- `sp`：TLS 并通过 ALPN 使用 HTTP/2；
- `sn`：TLS 且不发送 SNI；
- `h`：明文 HTTP/2（h2c）。

TLS 后缀还可使用 `端口s/协议/cipher-list` 指定该上游专用的 TLS 协议和 cipher list。省略时继承 `ssl_client`。例如：

```xml
<server name='h2_tls' proto='http' host='127.0.0.1' port='8443sp'/>
<server name='h2c' proto='http' host='127.0.0.1' port='8080h'/>
```

### 多节点

```xml
<server name='backend' proto='http' ip_hash='1'
        max_error_count='3' error_try_time='10'>
    <node host='10.0.0.11' port='8080' weight='2' life_time='30'/>
    <node host='10.0.0.12' port='8080' weight='1' life_time='30'/>
</server>
```

- `url_hash`：按 URL 选择节点；开启后优先于 `ip_hash`。
- `ip_hash`：按客户端 IP 选择节点。
- `cookie_stick`：启用 Cookie 黏着。
- `max_error_count`：节点达到该错误次数后临时下线。
- `error_try_time`：下线节点的重试间隔，秒。
- `node` 支持单节点的大部分连接属性，另有 `weight`。权重用于分配节点；不要把所有节点都设为 `0`。

## 外部程序和扩展

### `cmd`

`cmd` 由 kangle 启动外部程序并把请求交给它，只在启用虚拟主机运行身份支持的构建中注册。

```xml
<cmd name='php83' proto='fastcgi'
     file='/vhs/kangle/ext/php83/bin/php-cgi'
     type='sp' port='9000' life_time='60' idle_time='120'/>
```

| 属性 | 说明 |
| --- | --- |
| `name` | 唯一名称。 |
| `proto` | `http`、`fastcgi`、`ajp` 或 `tcp`。 |
| `file` | 可执行文件及命令行参数。 |
| `param` | 进程管理附加参数。 |
| `type` | `sp` 为单个常驻服务进程，`mp` 为由 extworker 管理的多进程模式。 |
| `port` | `sp` 模式下外部程序实际监听的端口；FastCGI 并不要求固定写成 `0`。 |
| `worker` | `mp` 模式工作进程数；非正数时按直连 stdin 模式处理。 |
| `life_time` | 上游连接复用时间。 |
| `idle_time` | 进程空闲退出时间。 |
| `max_request` | `mp` 模式下单个进程最多处理的请求数，达到后由 kangle 结束该进程，下一个请求启动新进程。未设置时，若 `<env>` 中 `PHP_FCGI_MAX_REQUESTS` 大于 0 则取该值。 |
| `max_error_count` | 最大容错次数。 |
| `chuser` | 是否按虚拟主机用户启动，除明确写 `0` 外默认开启。 |
| `sig` | Unix 上关闭进程所用信号，默认 `SIGKILL`。 |

子元素 `<env>` 的全部属性会作为环境变量；`<pre_event>` 和 `<post_event>` 可定义启动前后事件。事件配置属于高级功能，应优先参考管理界面生成的配置。

### `api`

加载 kangle API 扩展，例如 WebDAV 或内建管理模块：

```xml
<api name='webdav' file='bin/webdav.${dso}' life_time='60' max_error_count='5'/>
<api name='whm' file='buildin:whm'/>
```

常用属性为 `name`、`file`、`type`、`life_time`、`idle_time`、`max_request`、`max_error_count`；也支持 `<env>`、`<pre_event>`、`<post_event>`。普通动态库使用多线程模式；进程模式属于兼容/高级用法。

whm 包（`.whm` 文件）中的调用默认只能从管理端口执行；在普通虚拟主机上映射 `api:whm` 时，只有标记了 `scope='public'` 的调用会运行，其余返回 access denied。

### `dso_extend`

```xml
<dso_extend name='example' filename='bin/example.${dso}'/>
```

- `name`：唯一扩展名。
- `filename`：动态库路径，支持 `${dso}` 等动态变量。
- 其他属性原样提供给扩展自身解析。

`footer`、`param`、`param_count`、`post_file` 及其请求体/响应体过滤代码已经内置到 Kangle，不再需要 `filter.so` 或对应的 `dso_extend` 配置。旧的 `name='filter'` 加载项会被兼容忽略，配置迁移工具会将其删除。

DSO 开发接口见 [dso.md](./dso.md)。

### `vh_database`

虚拟主机数据库驱动配置应放在 `<config>` 根下：

```xml
<vh_database driver='bin/vhs_sqlite.${dso}' dbname='etc/vhs.db'/>
```

`driver` 是必填动态库路径，其余属性和子项由具体驱动解释。为兼容旧配置，放在 `<vhs>` 内仍可被发现，但新配置应使用根级形式。驱动必须导出 `initVirtualHostModule`，且其 ABI 必须与当前 kangle 匹配。

## 请求和响应规则

根级或 `<vh>` 内都可配置 `<request>`、`<response>`。详细模块列表和示例见 [ACL 与 Mark 参考](./acl_mark.md)。

```xml
<request action='vhs'>
    <table name='BEGIN'>
        <chain action='deny'>
            <acl module='path' path='/private/*'/>
        </chain>
    </table>
</request>
```

- `request`/`response` 的 `action` 是没有规则命中时的默认动作。
- `BEGIN` 是入口表。
- `response` 的 `POSTMAP` 表在映射物理文件路径后执行，适合 `file`、`dir`、`filename` 等文件 ACL。
- `chain@action` 支持 `allow`、`deny`、`drop`、`continue`、`return`/`default`、`table:名称`、`wback:名称`、`server:名称` 和 `proxy`。根级规则还支持 `vhs`、`api:名称`、`cmd:名称`、`dso:扩展:处理器` 等动作；虚拟主机局部规则不能跳转到这些全局目标。

## 虚拟主机全局项 `vhs`

`vhs` 保存可由 `vh inherit='1'` 继承的默认首页、错误页、MIME、别名和映射。

### `index`

可重复出现，顺序即查找顺序：

```xml
<index file='index.html'/>
<index file='index.htm'/>
```

### `error`

```xml
<error code='404' file='/404.html'/>
```

`code` 为 HTTP 状态码，`file` 为错误页 URL/路径。

### `mime_type`

| 属性 | 说明 |
| --- | --- |
| `ext` | 文件扩展名；`*` 为默认项。 |
| `type` | Content-Type。 |
| `compress` | `0` 未指定，`1` 允许压缩，`2` 禁止压缩。 |
| `max_age` | 该类型的缓存时间，秒。 |

### `alias`

```xml
<alias path='/assets' to='static' internal='0'/>
```

`path` 必须以 `/` 开头；`to` 为目标路径；`internal='1'` 表示只允许内部跳转访问。

### `map`

对外配置语法使用可重复的 `<map>`：

```xml
<map file_ext='php' extend='server:php' confirm_file='1' allow_method='GET,POST'/>
<map path='/dav' extend='api:webdav' confirm_file='0' allow_method='*'/>
```

- `file_ext`：按扩展名匹配；与 `path` 二选一。
- `path`：按 URL 路径前缀匹配。
- `extend`：`default`（静态文件）、`server:名称`、`cmd:名称`、`api:名称` 或 `dso:扩展:处理器`。
- `confirm_file`：`0` 不检查，`1` 要求文件存在，`2` 要求文件不存在。
- `allow_method`：允许的方法，`*` 表示全部，多个方法以逗号分隔。
- `params`：可选的上游参数，仅相应构建功能可用。

加载时 `<map>` 会在内部转换为 `map_file`/`map_path` 配置节点。直接写内部节点是兼容用法，新文档和新配置统一推荐 `<map>`。

## 虚拟主机 `vh`

```xml
<vh name='example' doc_root='/srv/www/example' inherit='1'
    browse='0' access='access.xml' htaccess='.htaccess'>
    <host>example.com</host>
    <host dir='www'>www.example.com</host>
    <bind>*:80</bind>
    <bind>*:443s</bind>
    <request action='allow'/>
</vh>
```

`<host>` 的 `dir` 也可以写成 `http://源站IP:端口/`，把该域名反向代理到源站；端口写 `0` 时跟随客户端访问的端口和协议。可用 URL 参数 `life_time=秒` 设置源站连接复用时间，不写时默认 60 秒，`life_time=0` 表示不复用。

### 常用属性

| 属性 | 说明 |
| --- | --- |
| `name` | 虚拟主机名称；填写时必须唯一。省略会生成运行期名称，但需要管理、更新或数据库关联的主机应显式设置。 |
| `doc_root` | 文档根目录，可为绝对路径或相对安装目录的路径。 |
| `browse` | 目录无首页时是否列目录，`0`/`1` 或 `off`/`on`。 |
| `inherit` | 是否继承 `vhs`，`0`/`1` 或 `off`/`on`。 |
| early_hints | 上游 103 Early Hints 转发，`0`/`1` 或 `off`/`on`/`h2` |
| `access` | 独立访问控制文件路径（相对路径基于 `doc_root`）。该文件通常在站点用户可写的目录里，只接受其中的 `<request>` 和 `<response>`，其他节点（如 `alias`、`map`、`error`、`env`）会被忽略并记录错误日志；文件中的 `%{env:名称}` 不会展开。 |
| `htaccess` | htaccess 文件名/路径。解析结果会缓存，文件修改后下一个请求即生效；缓存总量约 64MB，超出时淘汰最久未用的文件。 |
| `envs` | 虚拟主机环境变量列表，使用项目的环境变量格式。 |
| `status` | `0` 为正常，非 `0` 为关闭。 |
| `app`、`app_share`、`ip_hash` | 应用隔离、应用共享和哈希相关设置。 |
| `user`、`group` | 虚拟主机运行身份；Windows 下 `group` 兼作用户密码。Unix 下用户或用户组不存在（`-` 时为 `doc_root` 无法读取）时，该主机不会启动任何进程，相关请求返回错误。 |
| `chroot` | Unix 下是否 chroot，`0`/`1` 或 `off`/`on`。 |
| `speed_limit` | 主机总限速，支持大小单位。 |
| `max_connect` | 主机最大连接数。 |
| `max_worker`、`max_queue` | 主机工作者数和排队上限。 |
| `fflow` | 是否启用流量统计，`0`/`1`。 |
| `log_file` | 访问日志文件。 |
| `log_rotate_time`、`log_rotate_size` | 访问日志轮转时间和大小。 |
| `logs_day`、`logs_size` | 日志保留天数和总大小。 |
| `log_handle` | 是否启用日志处理器。 |
| `log_mkdir` | 是否自动创建日志目录；当前实现对该属性应写 `on`。 |

资源限制、独立日志、运行身份、队列和流量属性均受相应编译功能控制。

TLS 虚拟主机还可使用 `certificate`、`certificate_key`、`cipher`、`protocols`、`alpn`、`http2`、`http3` 和 `early_data`。`reject_nosni` 是监听器属性，不是虚拟主机属性。

### 子元素

- `<host>`：文本为域名，支持通配规则；`dir` 指定该域名相对 `doc_root` 的子目录。启用子虚拟主机证书功能时也可用 `certificate`、`certificate_key`。
- `<bind>`：文本为 `地址:端口`；只写端口会补成 `*:端口`。`s` 后缀表示 TLS，例如 `*:443s`。前导 `!` 是内部兼容标记，加载时会被去掉，新配置通常不需要写。
- `<index>`、`<error>`、`<mime_type>`、`<alias>`、`<map>`：语义同 `vhs`。
- `<request>`、`<response>`：该虚拟主机自己的访问控制规则。

## HTTP 回源请求体与 Trailer

已知请求体长度且没有 `Trailer` 请求头时，HTTP/1 回源使用 `Content-Length` 边接收边转发，
不按 64KiB 分界，也不会为了探测尾部字段而缓冲整个请求。长度未知或声明了 `Trailer` 时使用 chunked。

HTTP/2、HTTP/3 客户端向 HTTP/1 源站发送带 `Content-Length` 的请求并携带尾部字段时，
需在初始请求头声明，例如 `Trailer: X-Upload`。未声明就发送尾部字段会返回 400，
因为已经发送的 `Content-Length` 回源无法再附加 Trailer。HTTP/2 上游不需要这项声明。

## 配置排错

- 首先检查 XML 是否闭合、属性值是否正确转义（尤其是 `&` 应写为 `&amp;`）。
- 启动或重载后查看错误日志；未知上游协议会回退到 HTTP，需特别检查 `proto` 拼写。
- 映射目标名必须已定义并且加载顺序更早，例如 `extend='server:php'` 需要能找到名为 `php` 的 `server`。
- 修改监听器、线程数、缓存目录或扩展 ABI 后，优先完整重启而不是只做平滑重载。
