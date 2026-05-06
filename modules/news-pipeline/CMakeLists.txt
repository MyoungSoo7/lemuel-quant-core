add_executable(news_pipeline
    src/main.cpp
    src/rss_crawler.cpp
    src/ner.cpp
    src/sentiment.cpp
    src/finbert.cpp
    src/news_score.cpp
)

target_include_directories(news_pipeline PRIVATE include)
target_link_libraries(news_pipeline PRIVATE lqc_shared)

find_package(CURL REQUIRED)
target_link_libraries(news_pipeline PRIVATE CURL::libcurl)
target_compile_definitions(news_pipeline PRIVATE LQC_HAS_CURL=1)

# ONNX Runtime via pkg-config (Homebrew ships libonnxruntime.pc with the
# Cellar paths) → lib+header lookup as a backup.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ONNXRT QUIET libonnxruntime)
endif()
if(ONNXRT_FOUND)
    target_include_directories(news_pipeline PRIVATE ${ONNXRT_INCLUDE_DIRS})
    target_link_libraries(news_pipeline PRIVATE ${ONNXRT_LIBRARIES})
    target_link_directories(news_pipeline PRIVATE ${ONNXRT_LIBRARY_DIRS})
    target_compile_definitions(news_pipeline PRIVATE LQC_HAS_ONNX=1)
    message(STATUS "ONNX Runtime (pkg-config): ${ONNXRT_LIBRARIES}")
else()
    find_library(ONNX_LIB onnxruntime)
    find_path(ONNX_INC onnxruntime_cxx_api.h
        PATH_SUFFIXES onnxruntime onnxruntime/core/session)
    if(ONNX_LIB AND ONNX_INC)
        target_include_directories(news_pipeline PRIVATE ${ONNX_INC})
        target_link_libraries(news_pipeline PRIVATE ${ONNX_LIB})
        target_compile_definitions(news_pipeline PRIVATE LQC_HAS_ONNX=1)
        message(STATUS "ONNX Runtime: ${ONNX_LIB} (header: ${ONNX_INC})")
    else()
        message(STATUS "ONNX Runtime not found; news-pipeline lexicon fallback only")
    endif()
endif()
