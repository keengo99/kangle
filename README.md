# kangle
<img src="webadmin/logo.gif" alt="kangle logo"/>

kangle is a light, high-performance web server/reverse proxy.include a http manage console. Full support access control. memory/disk cache. and more kangle web server features
* fastcgi/http/http2 protocol upstream
* upstream keep alive
* memory and disk cache.
* full request/response access control
* easy used web manage console.
* each virtualhost can run seperate process and user.
* xml config file
* on the fly gzip/br/zstd compress
* dso extend.
* http3 support.

## build from source code
kangle use cmake to build.
```
git clone https://gitee.com/keengo/kangle
cd kangle
git submodule update --init --recursive
mkdir build
cd build
cmake ..
make
```
## build with http3 support
```

git clone https://github.com/litespeedtech/lsquic
cd lsquic && git submodule update --init --recursive
cd ..
git clone https://github.com/google/boringssl
git clone https://gitee.com/keengo/kangle
cd kangle
git submodule update --init --recursive
mkdir build
cd build
cmake .. -DBORINGSSL_DIR=../../boringssl -DLSQUIC_DIR=../../lsquic
make
```
## full stack test
note kangle test need golang support
### build test 
```
cd test
./build.sh
```
### run test
./test.sh

## cmake options
### brotli support.
* build use brotli source
`cmake .. -DBROTLI_DIR=brotli_dir`
* build use system brotli lib
`cmake .. -DENABLE_BROTLI=1`
### zstd support.
* build use zstd source
`cmake .. -DZSTD_DIR=zstd_dir`
* build use system zstd lib
`cmake .. -DENABLE_ZSTD=1`
### build release
`cmake .. -DCMAKE_BUILD_TYPE=Release`
### use boringssl
`cmake .. -DBORINGSSL_DIR=boringssl_dir`
### powered by boost fcontext
`cmake .. -DENABLE_FCONTEXT=1`
### build as proxy server
`cmake .. -DHTTP_PROXY=ON`

## Documentation

* [config](./docs/config.md).
* For develop dso, see the [DSO docs](./docs/dso.md).
