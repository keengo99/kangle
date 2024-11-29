# acl和mark模块
acl可以认为是匹配模块,mark是标记模块
各acl或mark模块可以使用
* `named_acl`和`named_mark`,用于定义命名模块,可用于一些共享模块,用以共享数据.父元素可以是`request`或`response`
* `acl`和`mark` 用于匿名模块,或引用命名模块,父元素是`chain`
* `module` 属性用于指示模块.或者`ref`用于引用命名模块.
* 用户可使用 `dso` 开发自定义的`acl`或`mark`模块.

## 内置acl匹配模块
### path
用于匹配url中的path部分.属性包含`path`和`raw`,`raw`指标原始path(有可能url会重定).
* `path` 指示`path`,大小写是否敏感取决于运行系统,windows是不敏感,其他系统是大小写敏感.
* `raw` 是否是原始path,0或1,默认为0
例如:
`<acl module='path' path='/index.php' />`
匹配重写后./index.php,
如匹配目录,则用*结尾.如:
`<acl module='path' path='/*' />`
### reg_path
使用正则表达式匹配url中的path部分.属性包含`path` `raw` `nc`
* `path` path的正则表达式写法
* `raw` 是否是原始path,0或1,默认为0
* `nc` 指示是否大小写敏感,0或1,默认为0, 1表示大小写不敏感,0表示大小写敏感.
例如:
`<acl module='reg_path' path='^/index\.php$' raw='1' nc='0' />`
表示匹配原始path为/index.php且大小写敏感.
### host
匹配主机名,大小写不敏感,属性名 `v`和`split`
* `v` 主机名称列表使用`split`指示的字符分隔.
* `split` 分割字符,只能单个字符,不能是字符串.
例如:
`<acl module='host' v='a.com|b.com|c.com' split='|'/>`
匹配a.com或b.com或c.com
### wide_host
支持泛域名形式的匹配主机名.大小写不敏感,属性名 `v`指示主机名称列表.用|分隔.
例如:
`<acl module='wide_host' v='*.a.com|b.com' />`
匹配 `*.a.com`或`b.com`,注意不会匹配`a.com`.而是`a.com`的子域名,如`www.a.com`


## 内置mark标记模块

## 未完待续