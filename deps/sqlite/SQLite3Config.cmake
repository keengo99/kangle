set(SQLite3_VERSION 3.54.1)

if(NOT TARGET SQLite::SQLite3)
    add_library(SQLite::SQLite3 SHARED IMPORTED)
    set_target_properties(SQLite::SQLite3 PROPERTIES
        IMPORTED_IMPLIB "${CMAKE_CURRENT_LIST_DIR}/lib/sqlite3.lib"
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/bin/sqlite3.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
    )
endif()
