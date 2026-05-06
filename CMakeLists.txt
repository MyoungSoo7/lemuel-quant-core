cmake_minimum_required(VERSION 3.20)
project(lemuel_quant_core
    VERSION 0.1.0
    DESCRIPTION "lemuel infra C++ core: market feeds, judge engine, data pipeline"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

option(LQC_BUILD_JUDGE_ENGINE   "Build codingtest judge engine"     ON)
option(LQC_BUILD_MARKET_FEED    "Build crypto market feed"          OFF)
option(LQC_BUILD_STOCK_FEED     "Build Korean stock feed"           OFF)
option(LQC_BUILD_DART_CRAWLER   "Build DART disclosure crawler"     OFF)
option(LQC_BUILD_NEWS_PIPELINE  "Build news NLP pipeline"           OFF)
option(LQC_BUILD_DATA_WAREHOUSE "Build data warehouse service"      OFF)

add_subdirectory(shared)

if(LQC_BUILD_JUDGE_ENGINE)
    add_subdirectory(modules/judge-engine)
endif()
if(LQC_BUILD_MARKET_FEED)
    add_subdirectory(modules/market-feed)
endif()
if(LQC_BUILD_STOCK_FEED)
    add_subdirectory(modules/stock-feed)
endif()
if(LQC_BUILD_DART_CRAWLER)
    add_subdirectory(modules/dart-crawler)
endif()
if(LQC_BUILD_NEWS_PIPELINE)
    add_subdirectory(modules/news-pipeline)
endif()
if(LQC_BUILD_DATA_WAREHOUSE)
    add_subdirectory(modules/data-warehouse)
endif()
