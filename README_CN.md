# kangle

[English](README.md) | 简体中文

<img src="webadmin/logo.gif" alt="kangle logo" width="180"/>

kangle 是一款轻量、高性能的 Web 服务器和反向代理软件，内置 Web
管理控制台，并提供灵活的请求/响应访问控制、虚拟主机、内存及磁盘缓存和多种上游协议支持。

## 主要功能

- 支持 HTTP/1.1、HTTP/2，以及可选的 HTTP/3
- 支持 HTTP、HTTP/2、FastCGI、AJP 上游及连接复用
- 支持内存缓存、磁盘缓存和大文件分段缓存
- 完整的请求和响应访问控制规则
- 虚拟主机进程及运行用户隔离
- XML 配置文件和 Web 管理控制台
- 支持动态 gzip、Brotli、Zstandard 压缩
- 支持 DSO 扩展、WebDAV 和基于 SQLite 的虚拟主机模块

## 编译要求

### Debian、Ubuntu

```bash
sudo apt update
sudo apt install -y \
  git build-essential cmake pkg-config \
  libssl-dev zlib1g-dev libpcre2-dev
```

### Rocky Linux、AlmaLinux、RHEL、Fedora

```bash
sudo dnf install -y \
  git gcc gcc-c++ make cmake pkgconf-pkg-config \
  openssl-devel zlib-devel pcre2-devel
```

### CentOS/RHEL 7

系统自带的 `cmake` 版本过低，需要安装 EPEL 提供的 `cmake3`，配置项目时也必须使用 `cmake3` 命令：

```bash
sudo yum install -y epel-release
sudo yum install -y \
  git gcc gcc-c++ make cmake3 \
  openssl-devel zlib-devel pcre2-devel
```

## 从源码编译

递归克隆仓库，`kasync` 和 `khttpd` 是必需的子模块。

```bash
git clone --recursive https://github.com/cccyun/kangle.git
cd kangle

mkdir cmake-build
cd cmake-build
cmake .. -DCMAKE_INSTALL_PREFIX=/vhs/kangle -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
```

CentOS/RHEL 7 需要将 `cmake` 替换为 `cmake3`：

```bash
cmake3 .. -DCMAKE_INSTALL_PREFIX=/vhs/kangle -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
```

如果克隆时没有添加 `--recursive`，请在配置前初始化子模块：

```bash
git submodule update --init --recursive
```

最终程序仍会输出到仓库的 `build/` 目录。可以用下面的命令检查已启用的功能：

```bash
../build/kangle -v
```

## 安装和运行

将 kangle 安装到 `/vhs/kangle`。如需修改安装位置，在执行 CMake 时添加
`-DCMAKE_INSTALL_PREFIX=/another/path`。

```bash
sudo make install
sudo install -d /vhs/kangle/var /vhs/kangle/ext /vhs/kangle/www
sudo cp -n /vhs/kangle/etc/config-default.xml /vhs/kangle/etc/config.xml
```

对外提供服务前，请检查 `/vhs/kangle/etc/config.xml` 中的监听地址和管理密码。默认配置监听
80 端口，管理接口仅监听 `127.0.0.1:3311`。

```bash
# 以守护进程方式启动
sudo /vhs/kangle/bin/kangle

# 平滑加载配置
sudo /vhs/kangle/bin/kangle -r

# 停止服务
sudo /vhs/kangle/bin/kangle -q

# 前台运行，适合容器或故障排查
sudo /vhs/kangle/bin/kangle -n -g
```

项目目前不会自动安装 systemd 服务单元。发行版安装包或自定义服务单元可以调用上面的命令。

## 可选编译功能

启用系统库前需要安装相应的开发包。常见包名包括
`libbrotli-dev`/`brotli-devel`、`libzstd-dev`/`libzstd-devel`、
`libjemalloc-dev`/`jemalloc-devel` 和 `liburing-dev`/`liburing-devel`。

| 选项 | 作用 |
| --- | --- |
| `-DENABLE_BROTLI=ON` | 使用系统 Brotli 库 |
| `-DBROTLI_DIR=/path/to/brotli` | 从指定源码目录编译 Brotli |
| `-DENABLE_ZSTD=ON` | 使用系统 Zstandard 库 |
| `-DZSTD_DIR=/path/to/zstd` | 从指定源码目录编译 Zstandard |
| `-DENABLE_JEMALLOC=ON` | 链接系统 jemalloc 库 |
| `-DLINUX_IOURING=ON` | 启用 Linux io_uring 支持 |
| `-DHTTP_PROXY=ON` | 编译以代理功能为主的版本 |
| `-DENABLE_DISK_CACHE=OFF` | 禁用磁盘缓存 |
| `-DENABLE_HTTP2=OFF` | 禁用 HTTP/2 |
| `-DKSOCKET_SSL=OFF` | 不编译 TLS 支持 |

例如，编译同时支持系统 Brotli 和 Zstandard 库的 Release 版本：

```bash
cmake .. \
  -DCMAKE_INSTALL_PREFIX=/vhs/kangle \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_BROTLI=ON \
  -DENABLE_ZSTD=ON
make -j"$(nproc)"
```

## 编译 HTTP/3

HTTP/3 同时需要 BoringSSL 和 LSQUIC 源码、CMake 3.16 或更高版本，以及支持 C11/C++17
的编译器。建议将依赖放在 kangle 仓库外。已验证的版本为 LSQUIC **v4.10.0** 和
BoringSSL **0.20250807.0**（提交 `e20e8486554098772673b55ea76bb003d0b330e5`）。
配置前必须应用仓库提供的两项 LSQUIC 补丁；CMake 会检查所需的头部 API 和补丁标记。
下面的 BoringSSL `checkout` 是可选的，用于复现已验证的构建，Kangle 并未限制必须使用该提交。
如需使用新版，可省略该命令或选择其他提交，然后重新编译并运行 TLS/HTTP/3 回归测试。

```bash
mkdir kangle-http3 && cd kangle-http3
git clone --recursive https://github.com/google/boringssl.git
# 可选：复现已验证的 BoringSSL 版本。
git -C boringssl checkout e20e8486554098772673b55ea76bb003d0b330e5
git clone --recursive --branch v4.10.0 https://github.com/litespeedtech/lsquic.git
git clone --recursive https://github.com/cccyun/kangle.git

cd kangle
git -C ../lsquic apply "$PWD/khttpd/patches/lsquic-4.10.0-content-length.patch"
git -C ../lsquic apply "$PWD/khttpd/patches/lsquic-4.10.0-reset-stream.patch"
cmake -S . -B cmake-build \
  -DCMAKE_INSTALL_PREFIX=/vhs/kangle \
  -DCMAKE_BUILD_TYPE=Release \
  -DKASYNC_DIR="$PWD/kasync" \
  -DBORINGSSL_DIR="$PWD/../boringssl" \
  -DLSQUIC_DIR="$PWD/../lsquic"
cmake --build cmake-build -j"$(nproc)"
```

不能只设置 `LSQUIC_DIR`；当前构建逻辑只有在同时设置 `BORINGSSL_DIR` 时才会启用 LSQUIC。

CentOS/RHEL 7 编译此版本需要更新的编译器。如果继续使用由 GCC 4.8 编译的第三方 C++
模块，请添加 `-DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=0"`，并在安装程序所在目录
提供兼容的 C++ 运行库，使用 `$ORIGIN` RPATH 加载。

在 HTTPS 监听配置中启用 `http3="1"`，并开放对应端口的 TCP 和 UDP。HTTP/3 使用 TLS 1.3。
可执行 `python3 test/http3_integration.py /absolute/path/to/kangle` 运行隔离的协议测试；
该测试需要 Go 1.23 和 Python `h2>=3.2`。

## 完整测试

集成测试要求 **Go 1.22 或更高版本**，第一次编译时需要下载 Go 模块。先完成 kangle
编译，然后执行：

```bash
cd test
./build.sh
./test.sh
```

测试程序会启动多个本地监听端口和辅助进程，请确认测试端口没有被其他程序占用。

## 文档

- [配置说明](docs/config.md)
- [DSO 扩展开发](docs/dso.md)
- [开源许可证](LICENSE)
