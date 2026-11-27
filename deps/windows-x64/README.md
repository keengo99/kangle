# Windows x64 编译依赖

Kangle、kwaf 和 kwebp 的 PowerShell 脚本默认使用这个目录中的头文件、Release 静态库和 WebP 的 CMake 配置。依赖来自同一套现有 vcpkg `x64-windows-static-md` 构建，使用 MSVC `/MD`。

| 依赖 | 版本 |
| --- | --- |
| OpenSSL | 3.5.9 |
| PCRE2 | 10.49 |
| zlib | 1.3.2 |
| Brotli | 1.2.1 |
| zstd | 1.5.7 |
| libwebp | 1.6.0 |

各包的版权说明保存在 `share` 下。SQLite 3.54.1 的导入库和 DLL 位于相邻的 `../sqlite`。如果要使用另一套兼容的 vcpkg 安装，可用 `-DependencyRoot` 指定它的 `installed/x64-windows-static-md` 目录。
