# Windows x64 SQLite SDK

这里保存同一批 vcpkg `x64-windows` 构建的 SQLite 3.54.1 Release 文件。`bin/sqlite3.dll` 与原运行目录中的 SQLite DLL 的 SHA-256 相同，导入库和头文件来自同一包。原包版权说明位于 `share/sqlite3/copyright`。

`SQLite3Config.cmake` 提供 `SQLite::SQLite3`。Kangle 和认证模块使用 `include` 下的头文件与 `lib/sqlite3.lib` 导入库，部署时共用 `bin/sqlite3.dll`。编译脚本从项目目录读取这套 SDK。
