# 配置文件
kangle配置支持xml格式的配置文件，主配置文件为`etc/config.xml`，可以在`ext`目录放置多个配置文件,支持多文件配置.
# 配置说明
* `属性` 指`xml`元素的属性，如`<img src='a.gif'/>` 其中`src`就是`img`其中之一`属性`。
* `文本内容` 指`xml`元素的`文本内容`，如`<sex>female</sex>`其中`female`就是`sex`的`文本内容` 
* `子元素` 如`<person><name>abc</name></person>` 其中`name`就是`person`的`子元素`
## 配置文件加载顺序
配置文件第一行设置加载顺序号.格式:
`<!--#start xxx -->`
其中`xxx`为顺序号数字，数字越小越先加载，主配置文件加载顺序为100。
## config
`config`为根元素，所有xml配置必须在`<config></config>`之中。
### listen
侦听端口,有如下属性：
* `ip` 侦听ip
* `port` 端口
* `type` 类型,可以有`http` `https` `manage` `manages` `tcp` `tcps`  


当`type`为`https` `manages` `tcps`时，可以设置如下ssl属性  
* `certificate` ssl公钥
* `certificate_key` ssl私钥
* `cipher` ssl算法
* `protocols` ssl协议
* `http2` 是否支持http2(0或1)
* `http3` 是否支持http3(0或1)
* `reject_nosni` 是否拒绝无sni的请求(0或1)
* `early_data` 是否支持early_data

例如:  
`<listen certificate='etc/server.crt' certificate_key='etc/server.key' cipher='' http2='1' ip='127.0.0.1' port='443' protocols='' reject_nosni='1' type='https'/>` 
### cache
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
### timeout
配置超时时间，`属性`:
* `rw` 读写超时时间,单位秒。默认60秒。
* `connect` 连接超时时间，单位秒,如缺失则同`rw`一样。
### compress
压缩配置，`属性`：
* `min_length` 最小压缩大小，对于已知大小的并且大于该设置的，将不会压缩.
* `only_cache` 是否只压缩可以被缓存的网页(0或1)，设置为1动态的将不压缩，默认是0.
* `gzip_level` gzip压缩级别，设置为0，不使用gzip压缩.
* `br_level` br压缩级别，设置为0，不使用br压缩
* `zstd_level` zstd压缩别，设置为0，不使用zstd压缩
### connect
连接相关配置，`属性`:
* `max` 最大连接数,默认是0不限制，超过则拒绝新连接。
* `max_per_ip` 每ip最大连接数，默认是0不限制，超过则拒绝该ip的新连接。
* `per_ip_deny` 超过`max_per_ip`时，是否加入黑名单(0或1),默认是0。
* `max_keep_alive` 最大长连接数，超过则使用短连接。
### vhs
虚拟主机全局配置,有如下`子元素`:
#### index
默认文件，`属性` `file` 设置文件名。
#### error
错误代码映射,`属性`:
* `code` 错误代码，如`403` `404`
* `file` 文件
#### mime_type
文件扩展名类型，`属性`:
* `ext` 文件扩展名,如 `html`
* `type` 文件类型，如：`text/html`
* `compress` 设置压缩(0,1,2)，0表示不明，1表示压缩，2表示永不压缩。默认是0。
* `max_age` 缓存时间，
####  alias
别名设置，`属性`:
* `path` 目录
* `to` 别名路径，注意不可以使用绝对路径(以/开头)
* `internal` 是否是内部别名(0或1)，默认是0，当别名是内部时，仅内部可见。
#### map_file
基于文件扩展名匹配扩展，看采用何种方式处理请求。`属性`:
* `ext` 文件扩展名
* `extend` 扩展,格式是`扩展类型<: 扩展名>`，扩展类型可以是:
1. `server` 单节点或多节点上游服务器。
2. `api` api扩展
3. `cmd` cmd扩展
4. `dso` dso扩展
5. `default` 默认，当静态文件直接发送。无扩展名。
* `confirm_file` 是否确认文件存在，0表示不确认，1表示确认存在，2表示确认不存在。默认是0不确认。
* `allow_method` 允许方法,`*`表示所有。多个方法用`,`隔开。如`GET,POST`
#### map_path
基于路径的匹配扩展，看用何种方式处理请求。`属性`:
* `path` 路径名
* `extend` `confirm_file` `allow_method`同`map_file`

### ...未完待续...