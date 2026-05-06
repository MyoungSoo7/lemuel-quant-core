add_executable(dart_crawler
    src/main.cpp
    src/dart_client.cpp
    src/disclosure_store.cpp
)

target_include_directories(dart_crawler PRIVATE include)
target_link_libraries(dart_crawler PRIVATE lqc_shared)

find_package(CURL QUIET)
if(CURL_FOUND)
    target_link_libraries(dart_crawler PRIVATE CURL::libcurl)
    target_compile_definitions(dart_crawler PRIVATE LQC_HAS_CURL=1)
endif()
