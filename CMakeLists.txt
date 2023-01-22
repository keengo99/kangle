cmake_minimum_required(VERSION 2.8.12) 
project(kangle)
set(KANGLE_VERSION "3.6.0")

set(CPACK_PACKAGE_VERSION ${KANGLE_VERSION})
set(KANGLE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_CXX_STANDARD 14)
file(GLOB KANGLE_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h  
    ${CMAKE_CURRENT_SOURCE_DIR}/module/access/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/whm/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/whm/whmdso/core/*.cpp    
    )
list(APPEND KANGLE_SOURCE_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/khttpd/davlib/KXml.cpp
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
    src/EdcodeUtils.cpp 
    src/KHttpExt.cpp 
    src/KApiDso.cpp 
    src/KApiEnv.cpp 
    src/KBuffer.cpp 
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
include(./cmake/FindPCRE.cmake)
list(APPEND KANGLE_INCLUDE_DIR ${PCRE_INCLUDE_DIRS}) 
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

if (ENABLE_BROTLI)
    check_include_file("brotli/encode.h" ENABLE_BROTLI)
    if (NOT ENABLE_BROTLI)
        message(FATAL_ERROR "cann't found brotli libary")
    else()
	    list(APPEND KANGLE_LIBS "brotlienc-static" "brotlicommon-static")
	    #      list(APPEND KANGLE_LIBS "brotlicommon" "brotlienc")
    endif()
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
    list(APPEND KANGLE_LIBS zdll pcre)
    list(APPEND EXTWORKER_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/KWinCgiEnv.cpp")
    list(APPEND VHS_SQLITE_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/module/vhs_sqlite/vhs_sqlite.def")
    set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdinclude")
else()
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
	include(FindZLIB)
	add_compile_options(-Wno-invalid-offsetof)
	if (NOT ZLIB_FOUND) 
		message(FATAL_ERROR "cann't found zlib")
	endif()
	list(APPEND KANGLE_LIBS ZLIB::ZLIB)
	list(APPEND KANGLE_LIBS ${PCRE_LIBRARIES})
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
include(FindSQLite3)
if (SQLite3_FOUND)
    list(APPEND KANGLE_LIBS SQLite::SQLite3)
    set(SQLITE3_LIB SQLite::SQLite3)
else()
    add_library(sqlite3 ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite/sqlite3.c)
    if (NOT(CMAKE_SYSTEM_NAME MATCHES "Windows"))
        target_compile_options(sqlite3 PUBLIC -fPIC)
    endif()
    target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite)
    list(APPEND KANGLE_LIBS sqlite3)
    set(SQLITE3_LIB sqlite3)
endif()

if (ENABLE_JEMALLOC)
	list(APPEND KANGLE_LIBS jemalloc)
endif()
message(STATUS "kangle version ${KANGLE_VERSION}")
configure_file(${KANGLE_DIR}/include/global.h.in ${KANGLE_DIR}/include/global.h @ONLY)
add_executable(kangle ${KANGLE_SOURCE_FILES} ${KHTTPD_INCLUDE_FILES})
list(APPEND KANGLE_LIBS khttpd)
target_include_directories(kangle PUBLIC ${KANGLE_INCLUDE_DIR})
target_link_directories(kangle PUBLIC ${KANGLE_LIBS_DIR})
#message(STATUS "kangle link libs ${KANGLE_LIBS} http_libs ${KHTTPD_LIBS} ${KASYNC_LIBS}")
target_link_libraries(kangle ${KANGLE_LIBS})
if (NOT HTTP_PROXY) 
    add_executable(extworker ${EXTWORKER_SOURCE_FILES})
    add_library(vhs_sqlite MODULE  ${VHS_SQLITE_SOURCE_FILES})
    add_library(testdso MODULE ${TESTDSO_SOURCE_FILES})
    add_library(filter MODULE ${FILTER_SOURCE_FILES})

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

    set_property(TARGET filter PROPERTY  PREFIX "")
    target_include_directories(filter PUBLIC ${KANGLE_INCLUDE_DIR})
    target_link_directories(filter PUBLIC ${KANGLE_LIBS_DIR})

    install(TARGETS extworker vhs_sqlite
     RUNTIME DESTINATION bin 
     LIBRARY DESTINATION bin
     )
endif()
add_subdirectory(module/webdav)
add_subdirectory(test/test_child)
add_dependencies(kangle khttpd)

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

include(CPack)
set(CPACK_PACKAGE_NAME "kangle")
