# kangle

[English](README.md) | [简体中文](README_CN.md)

<img src="webadmin/logo.gif" alt="kangle logo" width="180"/>

kangle is a lightweight, high-performance web server and reverse proxy with a
built-in web administration console. It provides flexible request/response
access control, virtual hosting, memory and disk caching, and extensible
upstream support.

## Features

- HTTP/1.1, HTTP/2, and optional HTTP/3
- HTTP, HTTP/2, FastCGI, and AJP upstreams with connection reuse
- Memory cache, disk cache, and partial caching for large objects
- Request and response access-control rules
- Per-virtual-host process and user isolation
- XML configuration and a web administration console
- On-the-fly gzip, Brotli, and Zstandard compression
- DSO extensions, WebDAV, and SQLite-backed virtual-host support

## Requirements

### Debian and Ubuntu

```bash
sudo apt update
sudo apt install -y \
  git build-essential cmake pkg-config \
  libssl-dev zlib1g-dev libpcre2-dev
```

### Rocky Linux, AlmaLinux, RHEL, and Fedora

```bash
sudo dnf install -y \
  git gcc gcc-c++ make cmake pkgconf-pkg-config \
  openssl-devel zlib-devel pcre2-devel
```

### CentOS/RHEL 7

The distribution's `cmake` package is too old. Install EPEL's `cmake3` package and use the `cmake3` command when configuring the project:

```bash
sudo yum install -y epel-release
sudo yum install -y \
  git gcc gcc-c++ make cmake3 \
  openssl-devel zlib-devel pcre2-devel
```

## Build from source

Clone the repository recursively; `kasync` and `khttpd` are required
submodules.

```bash
git clone --recursive https://github.com/cccyun/kangle.git
cd kangle

mkdir cmake-build
cd cmake-build
cmake .. -DCMAKE_INSTALL_PREFIX=/vhs/kangle -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
```

On CentOS/RHEL 7, replace `cmake` with `cmake3`:

```bash
cmake3 .. -DCMAKE_INSTALL_PREFIX=/vhs/kangle -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
```

If the repository was cloned without `--recursive`, initialize the submodules
before configuring:

```bash
git submodule update --init --recursive
```

Build products are placed in the repository's `build/` directory. Check the enabled features with:

```bash
../build/kangle -v
```

## Install and run

Install kangle under `/vhs/kangle`. Set
`-DCMAKE_INSTALL_PREFIX=/another/path` during configuration to choose a
different prefix.

```bash
sudo make install
sudo install -d /vhs/kangle/var /vhs/kangle/ext /vhs/kangle/www
sudo cp -n /vhs/kangle/etc/config-default.xml /vhs/kangle/etc/config.xml
```

Review `/vhs/kangle/etc/config.xml` before exposing the server, especially its
listeners and administration credentials. The sample configuration listens on
port 80 and exposes the management interface only on `127.0.0.1:3311`.

```bash
# Start as a daemon
sudo /vhs/kangle/bin/kangle

# Reload configuration gracefully
sudo /vhs/kangle/bin/kangle -r

# Stop the server
sudo /vhs/kangle/bin/kangle -q

# Run in the foreground (useful for containers and diagnostics)
sudo /vhs/kangle/bin/kangle -n -g
```

The project does not currently install a systemd unit. A distribution package
or a local service unit can call the commands above.

## Optional build features

Install the corresponding development package before enabling a system
library. Typical package names are `libbrotli-dev`/`brotli-devel`,
`libzstd-dev`/`libzstd-devel`, `libjemalloc-dev`/`jemalloc-devel`, and
`liburing-dev`/`liburing-devel`.

| Option | Purpose |
| --- | --- |
| `-DENABLE_BROTLI=ON` | Use the system Brotli libraries |
| `-DBROTLI_DIR=/path/to/brotli` | Build Brotli from a source tree |
| `-DENABLE_ZSTD=ON` | Use the system Zstandard library |
| `-DZSTD_DIR=/path/to/zstd` | Build Zstandard from a source tree |
| `-DENABLE_JEMALLOC=ON` | Link with the system jemalloc library |
| `-DLINUX_IOURING=ON` | Enable Linux io_uring support |
| `-DHTTP_PROXY=ON` | Build a proxy-focused variant |
| `-DENABLE_DISK_CACHE=OFF` | Disable disk-cache support |
| `-DENABLE_HTTP2=OFF` | Disable HTTP/2 support |
| `-DKSOCKET_SSL=OFF` | Build without TLS support |

For example, to build a release with the system Brotli and Zstandard
libraries:

```bash
cmake .. \
  -DCMAKE_INSTALL_PREFIX=/vhs/kangle \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_BROTLI=ON \
  -DENABLE_ZSTD=ON
make -j"$(nproc)"
```

## HTTP/3 build

HTTP/3 requires both BoringSSL and LSQUIC source trees, CMake 3.16 or newer,
and a compiler supporting C11/C++17. Keep the dependencies outside the kangle
repository. The tested versions are LSQUIC **v4.10.0** and BoringSSL
**0.20250807.0** (commit `e20e8486554098772673b55ea76bb003d0b330e5`).
Apply both bundled LSQUIC patches before configuring; CMake checks for the
required header API and patch markers.
The BoringSSL checkout below is optional and reproduces the tested build;
Kangle does not require that specific commit. To use a newer revision, omit
the checkout or select another commit, then rebuild and rerun TLS/HTTP/3 tests.

```bash
mkdir kangle-http3 && cd kangle-http3
git clone --recursive https://github.com/google/boringssl.git
# Optional: reproduce the tested BoringSSL revision.
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

Passing `LSQUIC_DIR` alone is not sufficient; the current build enables
LSQUIC only when `BORINGSSL_DIR` is also set.

CentOS/RHEL 7 needs a newer compiler for this build. When reusing third-party
C++ modules built with GCC 4.8, add
`-DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=0"` and provide a compatible C++
runtime alongside the installed binaries with an `$ORIGIN` RPATH.

Enable `http3="1"` on an HTTPS listener and allow both TCP and UDP on its port.
HTTP/3 uses TLS 1.3. The isolated protocol checks can be run with
`python3 test/http3_integration.py /absolute/path/to/kangle`; they require
Go 1.23 and Python `h2>=3.2`.

## Test suite

The integration tests require **Go 1.22 or newer** and download Go modules on
the first build. Build kangle first, then run:

```bash
cd test
./build.sh
./test.sh
```

The test runner starts local listeners and helper processes, so make sure its
test ports are not already in use.

## Documentation

- [Configuration](docs/config.md)
- [DSO development](docs/dso.md)
- [License](LICENSE)
