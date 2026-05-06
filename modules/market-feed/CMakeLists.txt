add_executable(market_feed
    src/main.cpp
    src/binance_client.cpp
    src/redis_publisher.cpp
)

target_include_directories(market_feed PRIVATE include)
target_link_libraries(market_feed PRIVATE lqc_shared)

# Boost (Beast / Asio) — header-only modules. brew's Boost 1.90 ships
# components as separate find_package targets; Beast lives under headers.
find_package(Boost REQUIRED CONFIG)
target_link_libraries(market_feed PRIVATE Boost::headers)
target_compile_definitions(market_feed PRIVATE LQC_HAS_BEAST=1)
find_package(Threads REQUIRED)
target_link_libraries(market_feed PRIVATE Threads::Threads)

find_package(OpenSSL REQUIRED)
target_link_libraries(market_feed PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_compile_definitions(market_feed PRIVATE LQC_HAS_OPENSSL=1)

# simdjson — required, header-include via package.
find_package(simdjson CONFIG REQUIRED)
target_link_libraries(market_feed PRIVATE simdjson::simdjson)
target_compile_definitions(market_feed PRIVATE LQC_HAS_SIMDJSON=1)

find_library(HIREDIS_LIB hiredis REQUIRED)
target_link_libraries(market_feed PRIVATE ${HIREDIS_LIB})
target_compile_definitions(market_feed PRIVATE LQC_HAS_HIREDIS=1)
