add_executable(market_feed
    src/main.cpp
    src/binance_client.cpp
    src/redis_publisher.cpp
)

target_include_directories(market_feed PRIVATE include)
target_link_libraries(market_feed PRIVATE lqc_shared)

# Optional deps — wired in when present so the module still builds in a
# minimal dev env. The skeleton ships header-stubs for the WS+Redis paths.
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_link_libraries(market_feed PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(market_feed PRIVATE LQC_HAS_OPENSSL=1)
endif()

find_library(HIREDIS_LIB hiredis)
if(HIREDIS_LIB)
    target_link_libraries(market_feed PRIVATE ${HIREDIS_LIB})
    target_compile_definitions(market_feed PRIVATE LQC_HAS_HIREDIS=1)
endif()
