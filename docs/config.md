# 配置文件
kangle配置支持xml格式的配置文件，主配置文件为`etc/config.xml`，可以在`ext`目录放置多个配置文件,支持多文件配置.
# 配置说明
* `属性` 指`xml`元素的属性，如`<img src='a.gif'/>` 其中`src`就是`img`其中之一`属性`。
* `文本内容` 指`xml`元素的`文本内容`，如`<sex>female</sex>`其中`female`就是`sex`的`文本内容` 
* `子元素` 如`<person><name>abc</name></person>` 其中`name`就是`person`的`子元素`
# 加载顺序
在多个配置文件存在时，可以在配置文件的第一行设置加载顺序.格式:
`<!--#start xxx -->`
其中`xxx`为顺序号数字，数字越小越先加载，主配置文件加载顺序为100。
# config
`config`为根元素，所有xml配置必须在`<config></config>`之中。
## listen
侦听端口,有如下`属性`：
* `ip` 侦听ip，如`*`,`127.0.0.1`
* `port` 端口
* `type` 类型,可以有`http` `https` `manage` `manages` `tcp` `tcps`  

>1. `http`/`https`，端口为http服务。
>2. `manage`/`manages`，端口为管理端口。
>3. `tcp`/`tcps`,端口为`tcp`四层转发
### ssl相关属性
> 当`type`为`https` `manages` `tcps`时，可以设置如下ssl属性  
* `certificate` ssl公钥
* `certificate_key` ssl私钥
* `cipher` ssl算法
* `protocols` ssl协议
* `http2` 是否支持http2(0或1)
* `http3` 是否支持http3(0或1)
* `early_data` 是否支持early_data
* `reject_nosni` 是否拒绝无sni的请求(0或1)，注意此配置仅在`listen`中有效。

例如:  
`<listen certificate='etc/server.crt' certificate_key='etc/server.key' cipher='' http2='1' ip='127.0.0.1' port='443' protocols='' reject_nosni='1' type='https'/>` 
## cache
配置缓存,kangle支持内存和磁盘两级缓存，也支持普通和智能缓存，智能缓存是指kangle缓存部分内容，可以合并多个部分缓存。
`属性`:
* `default` 默认是否缓存(0或1)
* `memory` 内存缓存大小,可以带单位`K` `M` `G`
* `disk` 磁盘缓存大小,可以带单位`K` `M` `G`,磁盘缓存在使用前需要格式化缓存目录.
* `disk_dir` 磁盘缓存目录,如为空，则表示主目录下`cache`目录。
* `refresh_time` 默认缓存时间,单位秒.
* `max_cache_size` 单个普通缓存文件大小，可以带单位`K` `M` `G`，如超过则开启智能缓存(中断了也可以缓存部分)。
* `max_bigobj_size` 单个最大智能缓存文件大小，可以带单位`K` `M` `G`，如超过将不缓存，一般的`max_bigobj_size`要大于`max_cache_size`
* `cache_part` 是否缓存部分内容(0或1),默认是开启1.
* `disk_work_time` 磁盘缓存清理时间，格式同`crontab`，可以设置一个时间段，用于集中清理磁盘缓存。

例如:  
`<cache cache_part='1' default='1' disk='1g' max_bigobj_size='1g' max_cache_size='256k' memory='1G' refresh_time='30'/>`
## timeout
配置超时时间，`属性`:
* `rw` 读写超时时间,单位秒。默认60秒。
* `connect` 连接超时时间，单位秒,如缺失则同`rw`一样。
## compress
压缩配置，`属性`：
* `min_length` 最小压缩大小，对于已知大小的并且小于该值的，将不会压缩.
* `only_cache` 是否只压缩可以被缓存的网页(0或1)，设置为1动态的将不压缩，默认是0.
* `gzip_level` gzip压缩级别，设置为0，不使用gzip压缩.
* `br_level` br压缩级别，设置为0，不使用br压缩
* `zstd_level` zstd压缩别，设置为0，不使用zstd压缩
## connect
连接相关配置，`属性`:
* `max` 最大连接数,默认是0不限制，超过则拒绝新连接。
* `max_per_ip` 每ip最大连接数，默认是0不限制，超过则拒绝该ip的新连接。
* `per_ip_deny` 超过`max_per_ip`时，是否加入黑名单(0或1),默认是0。
* `max_keep_alive` 最大长连接数，超过则使用短连接。
## request和response
> `request`请求控制，`response`为回应控制
> 每一个请求进来时，从`request`中的`BEGIN` `表`中开始匹配规则`目标`,直接有命中为止，如没有匹配，则使用`request`的`默认目标`
> 每一个请求回应之前，从 `response`中的`BEGIN` `表`中开始匹配规则`目标`,直接有命中为止，如没有匹配，则使用`response`的`默认目标`

`request`和`response`可以定义 `action` `属性`为`默认目标`,`action`可以为:
* `deny` 拒绝
* `allow` 接受
* `request`中可以为`server:名称` 反向代理到`名称`

`子元素`:
### named_acl和named_mark
定义命名模块，命名模块可以在`acl`和`mark`中引用

`属性`:
* `name` 模块id名字,要求唯一
* `module` 模块类型名称
* 各模块其他属性

### table
 `table`用于定义控制`表`，`name`属性指示唯一的名称,其中名称`BEGIN`为开始的默认表,`POSTMAP`在`response`中映射完物理路径后开始匹配规则的默认表。

 `子元素`:
#### chain
规则链,`属性` `action` 指示 `目标`,`子元素`:
##### acl和mark
定义匹配模块和标记模块,可以是引用`named_acl`和`named_mark`命名的模块，也可以定义匿名模块.
`ref`属性用于引用命名模块,如没有则定义匿名模块，其中`module`指示模块类型名称，其他属性用于各模块其他属性。
## vhs
虚拟主机全局配置,有如下`子元素`:
### index
默认文件，`属性` `file` 设置文件名。
### error
错误代码映射,`属性`:
* `code` 错误代码，如`403` `404`
* `file` 文件
### mime_type
文件扩展名类型，`属性`:
* `ext` 文件扩展名,如 `html`
* `type` 文件类型，如：`text/html`
* `compress` 设置压缩(0,1,2)，0表示不明，1表示压缩，2表示永不压缩。默认是0。
* `max_age` 缓存时间，
###  alias
别名设置，`属性`:
* `path` 目录
* `to` 别名路径，注意不可以使用绝对路径(以/开头)
* `internal` 是否是内部别名(0或1)，默认是0，当别名是内部时，仅内部可见。
### map_file
基于文件扩展名匹配扩展，看采用何种方式处理请求。`属性`:
* `ext` 文件扩展名
* `extend` 扩展,格式是`扩展类型<: 扩展名>`，扩展类型可以是:
1. `server` 单节点或多节点上游服务器。
2. `cmd` cmd扩展
3. `dso` dso扩展
4. `default` 默认，当静态文件直接发送。无扩展名。
* `confirm_file` 是否确认文件存在，0表示不确认，1表示确认存在，2表示确认不存在。默认是0不确认。
* `allow_method` 允许方法,`*`表示所有。多个方法用`,`隔开。如`GET,POST`
### map_path
基于路径的匹配扩展，看用何种方式处理请求。`属性`:
* `path` 路径名
* `extend` `confirm_file` `allow_method`同`map_file`
## vh
配置虚拟主机，有如下`属性`:
* `name` 虚拟主机名称，要求唯一。
* `doc_root` 主目录，可以是绝对地址，也可以是相对地址。
* `browse` 是否浏览(0或1,默认0),当url是目录时，没有默认首页时，是否列出文件和目录。
* `inherit` 是否继承(0或1,默认0),是否继承全局`vhs`的属性设置`index`,`error`,`mime_type`,`alias`,`map_file`,`map_path`。
* `user` 运行用户
* `group` 运行组,如果是windows系统，则表示`user`的密码。
* `speed_limit` 限速大小. 可以带单位`K`,`M`,`G`
* `max_connect` 最大连接数
* `status` 状态，0是正常，其他表示已经关闭。
* `max_worker`,`max_queue` 设置最大工作者和最大等待队列.
* `fflow` 是否开启流量统计(0或1,默认0)
* `certificate`,`certificate_key`, `cipher` ,`protocols`,`early_data`,`http2`,`http3` 等ssl相关属性可以参考[ssl相关属性](#ssl相关属性)

`vh`还有有如下`子元素`:
### host
绑定域名，可以有`属性``dir`表示子目录。
`文本内容`设置域名。
如:
```
<host dir='abc'>www.abc.com</host>
```
绑定`www.abc.com`域名到`abc`子目录
### bind
绑定到端口,`文本内容`设置端口格式为:`ip:port`,`ip`可以设置为`*`表示所有，`0.0.0.0`表示ipv4,`::`表示ipv6.
`port`可以加`s`,`t`等后缀，`s`表示ssl端口。`t`表示tcp端口。
如：
```
<bind>*:443s</bind>
```
表示绑定到443的ssl端口。
### index/error/alias/map_file/map_path
参考[vhs](#vhs)
### request和response
参考全局[request和response](#request和response)
## ...未完待续...