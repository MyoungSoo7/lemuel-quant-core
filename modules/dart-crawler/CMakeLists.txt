add_executable(dart_crawler
    src/main.cpp
    src/dart_client.cpp
    src/disclosure_store.cpp
)

target_include_directories(dart_crawler PRIVATE include)
target_link_libraries(dart_crawler PRIVATE lqc_shared)

find_package(CURL REQUIRED)
target_link_libraries(dart_crawler PRIVATE CURL::libcurl)
target_compile_definitions(dart_crawler PRIVATE LQC_HAS_CURL=1)

find_package(simdjson CONFIG REQUIRED)
target_link_libraries(dart_crawler PRIVATE simdjson::simdjson)
target_compile_definitions(dart_crawler PRIVATE LQC_HAS_SIMDJSON=1)

# libpqxx — PostgreSQL store. Optional; in-memory fallback always present.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBPQXX QUIET libpqxx)
    if(LIBPQXX_FOUND)
        target_include_directories(dart_crawler PRIVATE ${LIBPQXX_INCLUDE_DIRS})
        target_link_libraries(dart_crawler PRIVATE ${LIBPQXX_LIBRARIES})
        target_link_directories(dart_crawler PRIVATE ${LIBPQXX_LIBRARY_DIRS})
        target_compile_definitions(dart_crawler PRIVATE LQC_HAS_LIBPQXX=1)
    endif()
endif()
