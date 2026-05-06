add_executable(data_warehouse
    src/main.cpp
    src/timeseries_store.cpp
    src/parquet_writer.cpp
    src/r2_uploader.cpp
)

target_include_directories(data_warehouse PRIVATE include)
target_link_libraries(data_warehouse PRIVATE lqc_shared)

# Apache Arrow / Parquet — optional. Without it, parquet_writer ships
# a CSV fallback so the binary still builds in a minimal env.
find_package(Arrow CONFIG QUIET)
find_package(Parquet CONFIG QUIET)
if(Arrow_FOUND AND Parquet_FOUND)
    target_link_libraries(data_warehouse PRIVATE Arrow::arrow_shared
                                                  Parquet::parquet_shared)
    target_compile_definitions(data_warehouse PRIVATE LQC_HAS_PARQUET=1)
endif()

# AWS SDK for S3 (R2 is S3-compatible) — optional.
find_package(AWSSDK CONFIG COMPONENTS s3 QUIET)
if(AWSSDK_FOUND)
    target_link_libraries(data_warehouse PRIVATE ${AWSSDK_LINK_LIBRARIES})
    target_compile_definitions(data_warehouse PRIVATE LQC_HAS_AWS_S3=1)
endif()
