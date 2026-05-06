add_executable(stock_feed
    src/main.cpp
    src/kis_client.cpp
    src/krx_session.cpp
)

target_include_directories(stock_feed PRIVATE include)
target_link_libraries(stock_feed PRIVATE lqc_shared)

find_library(HIREDIS_LIB hiredis)
if(HIREDIS_LIB)
    target_link_libraries(stock_feed PRIVATE ${HIREDIS_LIB})
    target_compile_definitions(stock_feed PRIVATE LQC_HAS_HIREDIS=1)
endif()
