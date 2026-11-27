# Windows 编译

适用于 Windows x64 + Visual Studio 2022（MSVC v143）。下文命令都在 PowerShell 中、
从 Kangle 源码目录执行；kwaf、kwebp、auth_sqlite 需与 `kangle` 放在同一父目录下。

Windows Release 构建统一使用动态 VC 运行库（`/MD`）。安装程序需要安装与 MSVC 14.44
兼容的 Microsoft Visual C++ x64 Redistributable。此要求也适用于 `vhs_sqlite`、`kwaf`、
`kwebp`、`auth_sqlite`、`vhost`、`linxftp` 和 `runas`。

## 1. 准备工具

安装 Git 和 VS 2022 Build Tools 的 C++ 桌面开发工作负载（含 Windows SDK 和 CMake）：

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

拉取子模块：

```powershell
git submodule update --init --recursive
```

## 2. 安装依赖（仅首次）

依赖放在 `deps` 目录：

- `deps/vcpkg-overlay`：OpenSSL 3.5.9 的 vcpkg 覆盖端口，随仓库提交。
- `deps/windows-x64`：OpenSSL、PCRE2、zlib、Brotli、zstd、libwebp 的 Release 静态库（`/MD`）。
- `deps/sqlite`：SQLite 动态库，Kangle、vhs_sqlite、auth_sqlite 共用；`SQLite3Config.cmake` 随仓库提交。

`deps/vcpkg` 和上面两个目录里的库文件不提交（见 `deps/.gitignore`），按下面的步骤生成：

```powershell
$deps = "$PWD/deps"
git clone https://github.com/microsoft/vcpkg.git $deps/vcpkg
git -C $deps/vcpkg checkout --detach a4f7741c53c97354fc43222fea17955c657dd549
& $deps/vcpkg/bootstrap-vcpkg.bat -disableMetrics

$vcpkg = "$deps/vcpkg/vcpkg.exe"
& $vcpkg install openssl pcre2 zlib brotli zstd libwebp --triplet x64-windows-static-md "--overlay-ports=$deps/vcpkg-overlay" --classic
& $vcpkg install sqlite3 --triplet x64-windows --classic

# 静态库 -> deps/windows-x64
$static = "$deps/vcpkg/installed/x64-windows-static-md"
Copy-Item $static/include, $static/lib, $static/share $deps/windows-x64 -Recurse -Force

# SQLite -> deps/sqlite
$sqlite = "$deps/vcpkg/installed/x64-windows"
foreach ($f in 'include/sqlite3.h', 'include/sqlite3ext.h', 'include/sqlite3-vcpkg-config.h',
               'lib/sqlite3.lib', 'bin/sqlite3.dll', 'share/sqlite3/copyright') {
    New-Item -ItemType Directory -Force (Split-Path "$deps/sqlite/$f") | Out-Null
    Copy-Item "$sqlite/$f" "$deps/sqlite/$f" -Force
}
```

## 3. 编译

```powershell
.\build-windows.ps1
..\kwaf\build-windows.ps1
..\kwebp\build-windows.ps1
```

| 项目 | 产物 |
| --- | --- |
| Kangle | `build/install`（安装目录，含 `bin/sqlite3.dll` 和开发头文件；脚本会顺带跑单元测试） |
| kwaf / kwebp | 各自的 `build/Release/*.dll`，依赖先编好的 Kangle `build/install` |

脚本默认读取 `deps` 下的依赖。可用 `-DependencyRoot` 换一套依赖，`-BuildDirectory` 换构建目录，kwaf/kwebp 的 `-KangleDirectory` 指定 Kangle 安装目录。

构建为 `RelWithDebInfo`，启用 SSL、HTTP/2、Brotli、zstd，未启用 HTTP/3。复制到其他机器
需安装 x64 VC++ 运行库。

## 4. 运行

首次使用时把 `build/install/etc/config-default.xml` 复制为 `etc/config.xml`，修改管理员
密码和端口。前台运行 `bin/kangle.exe -n -g`，用 `bin/kangle.exe -q` 退出。

## 5. 测试（可选）

Go 集成测试（`-a 1` 测 HTTPS HTTP/2，`-a 3` 测 h2c；每轮使用新的 `-o` 目录）：

```powershell
$bin = "$PWD/build/install/bin"
$out = "$env:TEMP/kangle-go-test"
go build -C test/test_framework -o $out/kangle-tests.exe ./main
Copy-Item test/www $out/www -Recurse
& $out/kangle-tests.exe -e $bin -o $out -m=false -a 0
```

PHP FastCGI 与压测（需 Python 3 + `pip install h2`、OpenSSL 命令行和 php-cgi）：

```powershell
go build -C test/test_framework -o $out/kangle-bench.exe ./benchmark
$env:KANGLE_TEST_OPENSSL = 'C:/Program Files/Git/usr/bin/openssl.exe'
python test/php_fastcgi_integration.py $bin/kangle.exe <php-cgi.exe 路径> --benchmark $out/kangle-bench.exe
```
