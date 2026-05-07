add_executable(data_warehouse
    src/main.cpp
    src/timeseries_store.cpp
    src/parquet_writer.cpp
    src/r2_uploader.cpp
    src/rollup.cpp
    src/redis_subscriber.cpp
)

target_include_directories(data_warehouse PRIVATE include)
target_link_libraries(data_warehouse PRIVATE lqc_shared)

find_library(HIREDIS_LIB hiredis)
if(HIREDIS_LIB)
    target_link_libraries(data_warehouse PRIVATE ${HIREDIS_LIB})
    target_compile_definitions(data_warehouse PRIVATE LQC_HAS_HIREDIS=1)
endif()

# Apache Arrow / Parquet — required when this module is enabled.
# Without it, parquet_writer falls back to CSV (kept for CI builds without arrow).
find_package(Arrow CONFIG QUIET)
find_package(Parquet CONFIG QUIET)
if(Arrow_FOUND AND Parquet_FOUND)
    if(TARGET Arrow::arrow_shared)
        target_link_libraries(data_warehouse PRIVATE Arrow::arrow_shared)
    elseif(TARGET arrow_shared)
        target_link_libraries(data_warehouse PRIVATE arrow_shared)
    endif()
    if(TARGET Parquet::parquet_shared)
        target_link_libraries(data_warehouse PRIVATE Parquet::parquet_shared)
    elseif(TARGET parquet_shared)
        target_link_libraries(data_warehouse PRIVATE parquet_shared)
    endif()
    target_compile_definitions(data_warehouse PRIVATE LQC_HAS_PARQUET=1)
endif()

# AWS SDK for S3 (R2 is S3-compatible) — optional.
find_package(AWSSDK CONFIG COMPONENTS s3 QUIET)
if(AWSSDK_FOUND)
    target_link_libraries(data_warehouse PRIVATE ${AWSSDK_LINK_LIBRARIES})
    target_compile_definitions(data_warehouse PRIVATE LQC_HAS_AWS_S3=1)
endif()
