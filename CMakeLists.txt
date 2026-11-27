cmake_minimum_required(VERSION 2.8.12) 
project(kangle)
set(KANGLE_VERSION "3.6.0")

set(CPACK_PACKAGE_VERSION ${KANGLE_VERSION})
set(KANGLE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_CXX_STANDARD 17)

file(GLOB KANGLE_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h  
    ${CMAKE_CURRENT_SOURCE_DIR}/module/access/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/whm/*.cpp
    )
list(APPEND KANGLE_SOURCE_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/khttpd/davlib/KXml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/khttpd/davlib/KXmlDocument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/khttpd/davlib/KXmlContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/khttpd/davlib/KXmlException.cpp
    )
file(GLOB EXTWORKER_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/extworker/*.cpp
    )
file(GLOB VHS_SQLITE_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/module/vhs_sqlite/*.cpp
)
file(GLOB TESTDSO_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/module/testdso/*.cpp)
file(GLOB FILTER_SOURCE_FILES 
      ${CMAKE_CURRENT_SOURCE_DIR}/module/filter/*.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/module/filter/*.h)

list(APPEND EXTWORKER_SOURCE_FILES 
    src/KHttpExt.cpp 
    src/KApiDso.cpp 
    src/KApiEnv.cpp 
    src/KEnvInterface.cpp 
    src/KFastcgiUtils.cpp 
    src/KPipeStream.cpp
    src/KProcess.cpp 
    khttpd/src/KStream.cpp 
    khttpd/src/KStringBuf.cpp 
    src/KCgiEnv.cpp 
    src/process.cpp 
    src/KDynamicString.cpp
    khttpd/src/KAutoBuffer.cpp
    src/win_iconv.cpp
)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/module/whm)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/module/access)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/khttpd/davlib)

include(CheckSymbolExists)
include(CheckIncludeFile)

check_symbol_exists(timegm time.h HAVE_TIMEGM)
if (HAVE_TIMEGM)
    add_definitions(-DHAVE_TIMEGM)
endif()
#message(STATUS ${PCRE_INCLUDE_DIRS})
check_symbol_exists(KSOCKET_SSL ${KASYNC_INCLUDE_DIR}/kasync_config.h HAVE_KSOCKET_SSL)
set(KANGLE_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/kasync/build")
option(ENABLE_BROTLI "enable brotli compress support" OFF)
option(HTTP_PROXY "build kangle as proxy server" OFF)
option(ENABLE_DISK_CACHE "enable disk cache" ON)
option(ENABLE_MALLOCDEBUG "enable malloc debug" OFF)

if (NOT HTTP_PROXY)
    # The standard access filters are part of the server.  Only third-party
    # extensions use the runtime DSO registration path.
    list(APPEND KANGLE_SOURCE_FILES ${FILTER_SOURCE_FILES})
    list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/module/filter)
endif()

if (ENABLE_MALLOCDEBUG)
    add_compile_definitions(MALLOCDEBUG)
endif()
############# brotli ##########
if (BROTLI_DIR)
    get_filename_component(BROTLI_DIR ${BROTLI_DIR} REALPATH)
    add_subdirectory(${BROTLI_DIR} ${CMAKE_CURRENT_BINARY_DIR}/brotli EXCLUDE_FROM_ALL)
    list(APPEND KANGLE_LIBS "brotlienc-static" "brotlicommon-static")
    set(ENABLE_BROTLI ON)
elseif (ENABLE_BROTLI)
    find_path(BROTLI_INCLUDE_DIR brotli/encode.h)
    find_library(BROTLI_ENCODER_LIBRARY NAMES brotlienc)
    find_library(BROTLI_COMMON_LIBRARY NAMES brotlicommon)
    if (NOT BROTLI_INCLUDE_DIR OR NOT BROTLI_ENCODER_LIBRARY OR NOT BROTLI_COMMON_LIBRARY)
        message(FATAL_ERROR "cann't found brotli libary")
    else()
        list(APPEND KANGLE_INCLUDE_DIR ${BROTLI_INCLUDE_DIR})
        list(APPEND KANGLE_LIBS ${BROTLI_ENCODER_LIBRARY} ${BROTLI_COMMON_LIBRARY})
    endif()
endif()
########### zstd ##############
if (ZSTD_DIR)
    get_filename_component(ZSTD_DIR ${ZSTD_DIR} REALPATH)
    add_subdirectory(${ZSTD_DIR}/build/cmake ${CMAKE_CURRENT_BINARY_DIR}/zstd EXCLUDE_FROM_ALL)
    list(APPEND KANGLE_LIBS "libzstd_static")
    set(ENABLE_ZSTD ON)
elseif(ENABLE_ZSTD)
    find_path(ZSTD_INCLUDE_DIR zstd.h)
    find_library(ZSTD_LIBRARY NAMES zstd)
    if (NOT ZSTD_INCLUDE_DIR OR NOT ZSTD_LIBRARY)
        message(FATAL_ERROR "cann't found zstd libary")
    else()
        list(APPEND KANGLE_INCLUDE_DIR ${ZSTD_INCLUDE_DIR})
        list(APPEND KANGLE_LIBS ${ZSTD_LIBRARY})
    endif()
endif()
############## pcre2 ##############
if (PCRE2_DIR)
	get_filename_component(PCRE2_DIR ${PCRE2_DIR} REALPATH)
	add_subdirectory(${PCRE2_DIR} ${CMAKE_CURRENT_BINARY_DIR}/pcre2 EXCLUDE_FROM_ALL)
    set(PCRE2_LIBRARIES pcre2-posix-static)
    set(ENABLE_PCRE2 ON)
    list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/pcre2)
    list(APPEND KANGLE_LIBS ${PCRE2_LIBRARIES})
else()
    include(./cmake/FindPCRE2.cmake)
    if (PCRE2_FOUND)
        message(STATUS "found pcre2 lib")
        list(APPEND KANGLE_INCLUDE_DIR ${PCRE2_INCLUDE_DIRS})
        set(ENABLE_PCRE2 ON)
        list(APPEND KANGLE_LIBS ${PCRE2_LIBRARIES})
    else()
        ## try find pcre        
        include(./cmake/FindPCRE.cmake)
        if (NOT PCRE_FOUND)
            message(FATAL_ERROR "cann't found pcre2 or pcre lib")
        endif()
        message(STATUS "found pcre lib")
        list(APPEND KANGLE_INCLUDE_DIR ${PCRE_INCLUDE_DIRS})
        set(ENABLE_PCRE2 OFF)
        list(APPEND KANGLE_LIBS ${PCRE_LIBRARIES})
    endif()
endif()

if (NOT (CMAKE_SYSTEM_NAME MATCHES "Windows"))
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g2")
endif()
set(KASYNC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kasync)


#windows
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/build)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/build)
    list(APPEND KANGLE_LIBS ws2_32)
    
    INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/thirdinclude)
	LINK_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/win32lib/${CMAKE_VS_PLATFORM_NAME}")

    list(APPEND KANGLE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdinclude")
    list(APPEND KANGLE_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/win32lib/${CMAKE_VS_PLATFORM_NAME}")
    list(APPEND EXTWORKER_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/KWinCgiEnv.cpp")
    list(APPEND VHS_SQLITE_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/module/vhs_sqlite/vhs_sqlite.def")
    
else()
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)	
   
	if (CMAKE_SYSTEM_NAME MATCHES "Linux")
		list(APPEND KANGLE_LIBS dl)
    elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
        list(APPEND KANGLE_LIBS iconv)
	endif()	
endif()


add_subdirectory(khttpd)


#list(APPEND KANGLE_SOURCE_FILES ${KHTTPD_SOURCE_FILES})
list(APPEND KANGLE_INCLUDE_DIR ${KHTTPD_INCLUDE_DIR})
list(APPEND KANGLE_LIBS ${KHTTPD_LIBS})

############## sqlite3 ################
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(KANGLE_SQLITE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/sqlite" CACHE PATH
        "Windows x64 shared SQLite SDK")
endif()
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND EXISTS "${KANGLE_SQLITE_ROOT}/SQLite3Config.cmake")
    set(SQLite3_DIR "${KANGLE_SQLITE_ROOT}")
    find_package(SQLite3 CONFIG REQUIRED)
    set(KANGLE_SHARED_SQLITE ON)
else()
    find_package(SQLite3)
endif()
if (SQLite3_FOUND)
    list(APPEND KANGLE_LIBS SQLite::SQLite3)
    set(SQLITE3_LIB SQLite::SQLite3)
else()
    add_library(sqlite3 STATIC  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite/sqlite3.c)
    if (NOT(CMAKE_SYSTEM_NAME MATCHES "Windows"))
        target_compile_options(sqlite3 PUBLIC -fPIC)
    endif()
    target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite)
    list(APPEND KANGLE_LIBS sqlite3)
    set(SQLITE3_LIB sqlite3)
endif()
########### jemalloc #################
if (ENABLE_JEMALLOC)
	list(APPEND KANGLE_LIBS jemalloc)
endif()
message(STATUS "kangle version ${KANGLE_VERSION}")
configure_file(${KANGLE_DIR}/include/global.h.in ${KANGLE_DIR}/include/global.h @ONLY)
add_executable(kangle ${KANGLE_SOURCE_FILES} ${KHTTPD_INCLUDE_FILES})
if (WIN32)
	# cron.dll finds these with GetProcAddress, as cron.so does with dlsym.
	set_property(TARGET kangle APPEND_STRING PROPERTY LINK_FLAGS " /EXPORT:kfiber_create_sync /EXPORT:kfiber_join")
endif()
list(APPEND KANGLE_LIBS khttpd)
target_include_directories(kangle PUBLIC ${KANGLE_INCLUDE_DIR})
target_link_directories(kangle PUBLIC ${KANGLE_LIBS_DIR})
if (NOT (CMAKE_SYSTEM_NAME MATCHES "Windows"))
	target_compile_options(kangle PUBLIC -Wno-invalid-offsetof)
endif()
#message(STATUS "kangle link libs ${KANGLE_LIBS} http_libs ${KHTTPD_LIBS} ${KASYNC_LIBS}")
target_link_libraries(kangle ${KANGLE_LIBS})
if(KANGLE_SHARED_SQLITE)
    add_custom_command(TARGET kangle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SQLite::SQLite3>" "$<TARGET_FILE_DIR:kangle>"
        VERBATIM)
    install(FILES "$<TARGET_FILE:SQLite::SQLite3>" DESTINATION bin)
endif()
if (NOT HTTP_PROXY) 
    add_executable(extworker ${EXTWORKER_SOURCE_FILES})
    add_library(vhs_sqlite MODULE  ${VHS_SQLITE_SOURCE_FILES})
    add_library(testdso MODULE ${TESTDSO_SOURCE_FILES})

    target_include_directories(extworker PUBLIC ${KANGLE_INCLUDE_DIR})
    target_link_directories(extworker PUBLIC ${KANGLE_LIBS_DIR})
    target_link_libraries(extworker ${KANGLE_LIBS})

    set_property(TARGET vhs_sqlite PROPERTY  PREFIX "")
    target_include_directories(vhs_sqlite PUBLIC ${KANGLE_INCLUDE_DIR})
    target_link_directories(vhs_sqlite PUBLIC ${KANGLE_LIBS_DIR})
    target_link_libraries(vhs_sqlite ${SQLITE3_LIB})

    set_property(TARGET testdso PROPERTY  PREFIX "")
    target_include_directories(testdso PUBLIC ${KANGLE_INCLUDE_DIR})
    target_link_directories(testdso PUBLIC ${KANGLE_LIBS_DIR})

    install(TARGETS extworker vhs_sqlite
        RUNTIME DESTINATION bin 
        LIBRARY DESTINATION bin
    )
endif()
add_subdirectory(module/webdav)
add_subdirectory(test/test_child)
add_dependencies(kangle khttpd)
include(CTest)
if (BUILD_TESTING)
    add_executable(protocol_regression_test test/protocol_regression.cpp)
    target_include_directories(protocol_regression_test PRIVATE ${KDAVLIB_INCLUDE_DIR} ${KANGLE_INCLUDE_DIR})
    target_link_libraries(protocol_regression_test davlib khttpd)
    add_test(NAME protocol_regression COMMAND protocol_regression_test)
    add_executable(dav_lock_regression_test test/dav_lock_regression.cpp module/webdav/KDavLock.cpp)
    target_include_directories(dav_lock_regression_test PRIVATE ${KANGLE_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/module/webdav)
    target_link_libraries(dav_lock_regression_test khttpd)
    add_test(NAME dav_lock_regression COMMAND dav_lock_regression_test)
    add_library(api_functional_fixture MODULE test/api_functional_fixture.cpp)
    set_property(TARGET api_functional_fixture PROPERTY PREFIX "")
    target_include_directories(api_functional_fixture PRIVATE ${KANGLE_INCLUDE_DIR})
endif()
add_test(NAME "test_kangle" COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/test.sh)
set(CPACK_SOURCE_IGNORE_FILES "${PROJECT_BINARY_DIR};/.git/;.gitignore;/.vscode;/var;/test/bin;/test/cache;/test/var;/test/tmp;/test/ext;/test/webadmin")
install(TARGETS kangle
     RUNTIME DESTINATION bin 
     LIBRARY DESTINATION bin
     )

install(FILES 
     ${KANGLE_DIR}/include/ksapi.h 
     ${KANGLE_DIR}/kasync/include/kfeature.h    
     ${KANGLE_DIR}/kasync/include/kforwin32.h     
     ${KANGLE_DIR}/kasync/include/kasync_config.h
     ${KANGLE_DIR}/khttpd/include/khttp.h   
     TYPE INCLUDE)
install(DIRECTORY
    ${KANGLE_DIR}/webadmin/
    DESTINATION "webadmin"
    )
install(FILES
    ${KANGLE_DIR}/etc/config-default.xml
    DESTINATION etc)
    
set(CPACK_PACKAGE_NAME "kangle")
############ NSIS ####################
set(CPACK_NSIS_DISPLAY_NAME "kangle")
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "ExecWait '$INSTDIR/bin/kangle.exe --install'")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "ExecWait '$INSTDIR/bin/kangle.exe --uninstall'")
############ NSIS END ################
include(CPack)
