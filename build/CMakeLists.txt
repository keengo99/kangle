cmake_minimum_required(VERSION 2.8) 
project(kangle)
set(KANGLE_VERSION "3.6")

set(CPACK_PACKAGE_VERSION ${KANGLE_VERSION})
set(KANGLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../)

file(GLOB KANGLE_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/*.h  
    ${CMAKE_CURRENT_SOURCE_DIR}/../module/whm/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../module/whm/whmdso/core/*.cpp
    )
file(GLOB EXTWORKER_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../extworker/*.cpp
    )
file(GLOB VHS_SQLITE_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../module/vhs_sqlite/*.cpp
)
file(GLOB TESTDSO_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../module/testdso/*.cpp)

list(APPEND EXTWORKER_SOURCE_FILES 
    ../src/EdcodeUtils.cpp 
    ../src/KHttpExt.cpp 
    ../src/KApiDso.cpp 
    ../src/KApiEnv.cpp 
    ../src/KBuffer.cpp 
    ../src/KEnvInterface.cpp 
    ../src/KFastcgiUtils.cpp 
    ../src/KPipeStream.cpp
    ../src/KProcess.cpp 
    ../src/KStream.cpp 
    ../src/KStringBuf.cpp 
    ../src/KCgiEnv.cpp 
    ../src/process.cpp 
    ../src/KAutoBuffer.cpp
    ../src/win_iconv.cpp
)
set(KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../kasync/include)
set(KASYNC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../kasync/include)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../module/whm)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../module/access)
include(CheckSymbolExists)
include(CheckIncludeFile)

check_symbol_exists(KSOCKET_SSL ${KASYNC_INCLUDE_DIR}/kasync_config.h HAVE_KSOCKET_SSL)
set(KANGLE_LIBS kasync)
set(KANGLE_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../kasync/build")
option(ENABLE_BROTLI "enable brotli compress support" OFF)
option(HTTP_PROXY "build kangle as proxy server" OFF)

if (ENABLE_BROTLI)
    check_include_file("brotli/encode.h" ENABLE_BROTLI)
    if (NOT ENABLE_BROTLI)
        message(FATAL_ERROR "cann't found brotli libary")
    else()
        list(APPEND KANGLE_LIBS "brotlicommon-static" "brotlienc-static")
    endif()
endif()
#openssl
if (HAVE_KSOCKET_SSL)
    include(FindOpenSSL)
    if (NOT OPENSSL_FOUND)
        message(FATAL_ERROR "cann't found ssl")
    endif() 
    list(APPEND KANGLE_LIBS  OpenSSL::SSL OpenSSL::Crypto)
    list(APPEND KANGLE_INCLUDE_DIR ${OPENSSL_INCLUDE_DIR}) 
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
#windows
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG  ${CMAKE_CURRENT_SOURCE_DIR}/../Debug)
    set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/../Debug)
    list(APPEND KANGLE_LIBS ws2_32)
    list(APPEND KANGLE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../thirdinclude")
    list(APPEND KANGLE_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../win32lib/${CMAKE_VS_PLATFORM_NAME}")
    list(APPEND KANGLE_LIBS zdll pcre)
    list(APPEND EXTWORKER_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../src/KWinCgiEnv.cpp")
    list(APPEND VHS_SQLITE_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../module/vhs_sqlite/vhs_sqlite.def")
endif()
if (CMAKE_SYSTEM_NAME MATCHES "Linux") 
    #linux
	add_compile_options(-Wno-invalid-offsetof)
    check_symbol_exists(LINUX_IOURING ${KASYNC_INCLUDE_DIR}/kasync_config.h HAVE_LINUX_IOURING)
    if (HAVE_LINUX_IOURING)
        message(STATUS "found io_uring")
        list(APPEND KANGLE_LIBS uring)
    endif()
    list(APPEND KANGLE_LIBS pthread dl sqlite3 pcre z)
endif()

message(STATUS "kangle version ${KANGLE_VERSION}")
configure_file(${KANGLE_DIR}/include/global.h.in ${KANGLE_DIR}/include/global.h @ONLY)

add_executable(kangle ${KANGLE_SOURCE_FILES})
target_include_directories(kangle PUBLIC ${KANGLE_INCLUDE_DIR})
target_link_directories(kangle PUBLIC ${KANGLE_LIBS_DIR})
target_link_libraries(kangle ${KANGLE_LIBS} )
install(TARGETS kangle
     RUNTIME DESTINATION bin 
     LIBRARY DESTINATION bin
     )

if (NOT HTTP_PROXY) 
    add_executable(extworker ${EXTWORKER_SOURCE_FILES})
    add_library(vhs_sqlite MODULE  ${VHS_SQLITE_SOURCE_FILES})
    add_library(testdso MODULE ${TESTDSO_SOURCE_FILES})

    target_include_directories(extworker PUBLIC ${KANGLE_INCLUDE_DIR})
    target_link_directories(extworker PUBLIC ${KANGLE_LIBS_DIR})
    target_link_libraries(extworker ${KANGLE_LIBS} )

    set_property(TARGET vhs_sqlite PROPERTY  PREFIX "")
    target_include_directories(vhs_sqlite PUBLIC ${KANGLE_INCLUDE_DIR})
    target_link_directories(vhs_sqlite PUBLIC ${KANGLE_LIBS_DIR})

    set_property(TARGET testdso PROPERTY  PREFIX "")
    target_include_directories(testdso PUBLIC ${KANGLE_INCLUDE_DIR})
    target_link_directories(testdso PUBLIC ${KANGLE_LIBS_DIR})
    install(TARGETS extworker vhs_sqlite
     RUNTIME DESTINATION bin 
     LIBRARY DESTINATION bin
     )
endif()

install(FILES 
     ${KANGLE_DIR}/include/ksapi.h 
     ${KANGLE_DIR}/kasync/include/kfeature.h    
     ${KANGLE_DIR}/kasync/include/kforwin32.h     
     ${KANGLE_DIR}/kasync/include/kasync_config.h
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
