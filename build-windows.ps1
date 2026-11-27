param(
    [string]$BuildDirectory = '',
    [string]$DependencyRoot = ''
)

$ErrorActionPreference = 'Stop'
if (!$BuildDirectory) { $BuildDirectory = Join-Path $PSScriptRoot 'build' }
if (!$DependencyRoot) { $DependencyRoot = Join-Path $PSScriptRoot 'deps/windows-x64' }
$BuildDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDirectory)
$DependencyRoot = (Resolve-Path -LiteralPath $DependencyRoot).Path

$command = Get-Command cmake.exe -ErrorAction SilentlyContinue
if ($command) {
    $cmake = $command.Source
} else {
    $cmake = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
}
if (!(Test-Path -LiteralPath $cmake -PathType Leaf)) {
    throw 'Install Visual Studio 2022 C++ x64 Build Tools, Windows SDK and CMake.'
}

foreach ($library in 'libssl.lib', 'libcrypto.lib', 'pcre2-8.lib', 'zs.lib', 'brotlienc.lib', 'brotlicommon.lib', 'zstd.lib') {
    if (!(Test-Path -LiteralPath (Join-Path $DependencyRoot "lib/$library") -PathType Leaf)) {
        throw "Missing $library in Windows x64 SDK: $DependencyRoot"
    }
}
$sqliteRoot = Join-Path $PSScriptRoot 'deps/sqlite'
if (!(Test-Path -LiteralPath (Join-Path $sqliteRoot 'SQLite3Config.cmake') -PathType Leaf)) {
    throw "Missing shared SQLite SDK: $sqliteRoot"
}

$installDirectory = Join-Path $BuildDirectory 'install'
$configure = @(
    '-S', $PSScriptRoot, '-B', $BuildDirectory, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    '-DCMAKE_POLICY_VERSION_MINIMUM=3.5', '-DCMAKE_CONFIGURATION_TYPES=RelWithDebInfo',
    '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
    '-DCMAKE_CXX_FLAGS=/utf-8 /EHsc /DPCRE2_STATIC /MP4', '-DCMAKE_C_FLAGS=/utf-8 /MP4',
    "-DCMAKE_PREFIX_PATH=$DependencyRoot", "-DOPENSSL_ROOT_DIR=$DependencyRoot",
    '-DOPENSSL_USE_STATIC_LIBS=ON', "-DZLIB_LIBRARY_RELEASE=$DependencyRoot/lib/zs.lib",
    "-DCMAKE_REQUIRED_INCLUDES=$DependencyRoot/include", "-DKANGLE_SQLITE_ROOT=$sqliteRoot",
    "-DCMAKE_INSTALL_PREFIX=$installDirectory", '-DENABLE_BROTLI=ON', '-DENABLE_ZSTD=ON',
    '-DBUILD_TESTING=ON'
)
& $cmake @configure
if ($LASTEXITCODE -ne 0) { throw 'Kangle CMake configuration failed' }
& $cmake --build $BuildDirectory --config RelWithDebInfo --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Kangle compilation failed' }

$ctest = Join-Path (Split-Path $cmake -Parent) 'ctest.exe'
& $ctest --test-dir $BuildDirectory -C RelWithDebInfo -R '^(protocol_regression|dav_lock_regression)$' --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Kangle regression tests failed' }

& $cmake --install $BuildDirectory --config RelWithDebInfo --prefix $installDirectory
if ($LASTEXITCODE -ne 0) { throw 'Kangle SDK staging failed' }
Write-Output (Join-Path $installDirectory 'bin/kangle.exe')
