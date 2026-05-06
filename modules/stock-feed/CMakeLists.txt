add_executable(stock_feed
    src/main.cpp
    src/kis_client.cpp
    src/krx_session.cpp
)

target_include_directories(stock_feed PRIVATE include)
target_link_libraries(stock_feed PRIVATE lqc_shared)

find_package(Boost REQUIRED CONFIG)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
target_link_libraries(stock_feed PRIVATE Boost::headers Threads::Threads
                                          OpenSSL::SSL OpenSSL::Crypto)
target_compile_definitions(stock_feed PRIVATE LQC_HAS_BEAST=1
                                               LQC_HAS_OPENSSL=1)

find_package(simdjson CONFIG REQUIRED)
target_link_libraries(stock_feed PRIVATE simdjson::simdjson)
target_compile_definitions(stock_feed PRIVATE LQC_HAS_SIMDJSON=1)

find_package(CURL REQUIRED)
target_link_libraries(stock_feed PRIVATE CURL::libcurl)
target_compile_definitions(stock_feed PRIVATE LQC_HAS_CURL=1)

find_library(HIREDIS_LIB hiredis REQUIRED)
target_link_libraries(stock_feed PRIVATE ${HIREDIS_LIB})
target_compile_definitions(stock_feed PRIVATE LQC_HAS_HIREDIS=1)
