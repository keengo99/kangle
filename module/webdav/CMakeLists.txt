
file(GLOB KWEBDAV_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    )

set(KWEBDAV_INCLUDE_DIRS  ${CMAKE_CURRENT_SOURCE_DIR})
if (CMAKE_SYSTEM_NAME  MATCHES "Windows")
  
else()

endif()

include(CheckIncludeFiles)
list(APPEND CMAKE_REQUIRED_INCLUDES ${KANGLE_DIR}/include)
list(APPEND CMAKE_REQUIRED_INCLUDES ${KANGLE_DIR}/kasync/include)
list(APPEND CMAKE_REQUIRED_INCLUDES ${KANGLE_DIR}/khttpd/include)


check_include_files("ksapi.h;kfeature.h;kforwin32.h" FOUND_KSAPI_HEADER)
if (NOT FOUND_KSAPI_HEADER)
    message(STATUS "include dir = ${CMAKE_REQUIRED_INCLUDES}")
    message(FATAL_ERROR "cann't found ksapi.h. try cmake -DKANGLE_DIR=kangle_dir")
endif()
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/khttpd/davlib/KXml.cpp)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/khttpd/davlib/KXmlException.cpp)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/khttpd/davlib/KXmlContext.cpp)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/khttpd/davlib/KXmlDocument.cpp)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/src/KISAPIServiceProvider.cpp)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/src/KServiceProvider.cpp)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/src/directory.cpp)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/khttpd/src/KStringBuf.cpp)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/khttpd/src/KStream.cpp)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/khttpd/src/KHttpLib.cpp)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/khttpd/src/KHttpKeyValue.cpp)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/kasync/src/kfile.c)
list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/kasync/src/krbtree.c)

add_library(webdav MODULE  ${KWEBDAV_SOURCE_FILES})
set_property(TARGET webdav PROPERTY  PREFIX "")

target_include_directories(webdav PUBLIC ${CMAKE_REQUIRED_INCLUDES} ${KWEBDAV_INCLUDE_DIRS} ${KANGLE_DIR}/khttpd/davlib)

set(CMAKE_INSTALL_PREFIX ${KANGLE_DIR})
install(TARGETS webdav LIBRARY DESTINATION bin)

