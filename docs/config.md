# 配置文件
kangle配置支持xml格式的配置文件，主配置文件为`etc/config.xml`，可以在`ext`目录放置多个配置文件,支持多文件配置.
## 配置文件加载顺序
配置文件第一行设置加载顺序号.格式:
`<!--#start xxx -->`
其中`xxx`为顺序号数字，数字越小越先加载，主配置文件加载顺序为100。
## config
`config`为根节点，所有xml配置必须在`<config></config>`之中。
## listen 侦听端口
`listen`有如下属性：
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
### cache 缓存
kangle支持内存和磁盘两级缓存，也支持普通和智能缓存，智能缓存是指kangle缓存部分内容，可以合并多个部分缓存。
`cache`有如下属性:
例如:
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
### ...未完待续...