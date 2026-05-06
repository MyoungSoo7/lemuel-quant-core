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

find_package(CURL QUIET)
if(CURL_FOUND)
    target_link_libraries(news_pipeline PRIVATE CURL::libcurl)
    target_compile_definitions(news_pipeline PRIVATE LQC_HAS_CURL=1)
endif()

# Optional: ONNX Runtime for KoBERT inference. Search lib + header at link time.
find_library(ONNX_LIB onnxruntime)
find_path(ONNX_INC onnxruntime_cxx_api.h
    PATH_SUFFIXES onnxruntime onnxruntime/core/session)
if(ONNX_LIB AND ONNX_INC)
    target_include_directories(news_pipeline PRIVATE ${ONNX_INC})
    target_link_libraries(news_pipeline PRIVATE ${ONNX_LIB})
    target_compile_definitions(news_pipeline PRIVATE LQC_HAS_ONNX=1)
endif()
