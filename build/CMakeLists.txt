cmake_minimum_required(VERSION 3.10) 
project(kangle)
file(GLOB KANGLE_SOURCE_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../module/whm/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../module/whm/whmdso/core/*.cpp
    )

set(KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../kasync/include)
set(KASYNC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../kasync/include)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../module/whm)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../module/access)
include(CheckSymbolExists)

check_symbol_exists(KSOCKET_SSL ${KASYNC_INCLUDE_DIR}/kasync_config.h HAVE_KSOCKET_SSL)
set(KANGLE_LIBS kasync)
set(KANGLE_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../kasync/build")
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
#windows
    list(APPEND KANGLE_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../win32lib/${CMAKE_VS_PLATFORM_NAME}")
    list(APPEND KANGLE_LIBS zdll pcre)
    list(APPEND KANGLE_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../win32lib/win_iconv.cpp")
else()
#not windows
    check_symbol_exists(LINUX_IOURING ${KASYNC_INCLUDE_DIR}/kasync_config.h HAVE_LINUX_IOURING)
    if (HAVE_LINUX_IOURING)
        message(STATUS "found io_uring")
        list(APPEND KANGLE_LIBS uring)
    endif()
endif()


if (HAVE_KSOCKET_SSL)
    include(FindOpenSSL)
    if (NOT OPENSSL_FOUND)
        message(FATAL_ERROR "cann't found ssl")
    endif()
 
    list(APPEND KANGLE_LIBS  OpenSSL::SSL OpenSSL::Crypto)
    list(APPEND KANGLE_INCLUDE_DIR ${OPENSSL_INCLUDE_DIR}) 
endif()
if (CMAKE_SYSTEM_NAME MATCHES "Linux") 
    list(APPEND KANGLE_LIBS pthread)
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    list(APPEND KANGLE_LIBS ws2_32)
    list(APPEND KANGLE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../thirdinclude")
endif()

add_executable(kangle ${KANGLE_SOURCE_FILES})
target_include_directories(kangle PUBLIC ${KANGLE_INCLUDE_DIR})
target_link_directories(kangle PUBLIC ${KANGLE_LIBS_DIR})
target_link_libraries(kangle ${KANGLE_LIBS} )
