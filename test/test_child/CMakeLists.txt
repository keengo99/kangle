file(GLOB KWEBDAV_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    )

set(TEST_CHILD_INCLUDE_DIRS  ${CMAKE_CURRENT_SOURCE_DIR})
if (CMAKE_SYSTEM_NAME  MATCHES "Windows")
  
else()

endif()

include(CheckIncludeFiles)

list(APPEND CMAKE_REQUIRED_INCLUDES ${KANGLE_DIR}/kasync/include)

check_include_files("kfeature.h;kforwin32.h" FOUND_KSAPI_HEADER)
if (NOT FOUND_KSAPI_HEADER)
    message(STATUS "include dir = ${CMAKE_REQUIRED_INCLUDES}")
    message(FATAL_ERROR "cann't found kfeature.h. try cmake -DKANGLE_DIR=kangle_dir")
endif()
#list(APPEND KWEBDAV_SOURCE_FILES ${KANGLE_DIR}/kasync/src/ksocket.c)

add_executable(test_child  ${KWEBDAV_SOURCE_FILES} ${KANGLE_DIR}/src/KEnvInterface.cpp)
target_link_directories(test_child PUBLIC ${KANGLE_LIBS_DIR})
target_link_libraries(test_child ${KHTTPD_LIBS} khttpd davlib)

target_include_directories(test_child PUBLIC ${CMAKE_REQUIRED_INCLUDES} ${TEST_CHILD_INCLUDE_DIRS} ${KHTTPD_INCLUDE_DIR} ${KANGLE_INCLUDE_DIR})

set(CMAKE_INSTALL_PREFIX ${KANGLE_DIR})
install(TARGETS test_child LIBRARY DESTINATION bin)

