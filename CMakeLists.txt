cmake_minimum_required(VERSION 3.16)
project(GreedyCpuScheduling LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SCHED_SANITIZE "Build with AddressSanitizer + UBSan" OFF)

add_library(scheduler_core
    src/context.cpp
    src/registry.cpp
    src/engine.cpp
    src/report.cpp
    src/algorithms/fcfs.cpp
    src/algorithms/round_robin.cpp
    src/algorithms/shortest_job.cpp
    src/algorithms/hrrn.cpp
    src/algorithms/feedback.cpp
    src/algorithms/priority.cpp
)
target_include_directories(scheduler_core PUBLIC include)

if(NOT MSVC)
    target_compile_options(scheduler_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(SCHED_SANITIZE)
    target_compile_options(scheduler_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(scheduler_core PUBLIC -fsanitize=address,undefined)
endif()

if(EMSCRIPTEN)
    # Browser build: ES module exposing simulate / legacyReport / benchmark / catalog.
    target_compile_options(scheduler_core PUBLIC -fexceptions)
    add_executable(scheduler_wasm src/wasm.cpp)
    target_link_libraries(scheduler_wasm PRIVATE scheduler_core)
    set_target_properties(scheduler_wasm PROPERTIES OUTPUT_NAME "scheduler" SUFFIX ".mjs")
    target_link_options(scheduler_wasm PRIVATE
        -fexceptions -lembind -O3
        -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createSchedulerModule
        -sENVIRONMENT=web,worker,node -sALLOW_MEMORY_GROWTH=1 -sSINGLE_FILE=1)
endif()

add_executable(scheduler app/main.cpp)
target_link_libraries(scheduler PRIVATE scheduler_core)

enable_testing()
add_executable(scheduler_tests tests/test_main.cpp)
target_link_libraries(scheduler_tests PRIVATE scheduler_core)
if(EMSCRIPTEN)
    target_link_options(scheduler_tests PRIVATE -fexceptions -sNODERAWFS=1)
    target_link_options(scheduler PRIVATE -fexceptions -sNODERAWFS=1)
endif()
add_test(NAME scheduler_tests COMMAND scheduler_tests ${CMAKE_SOURCE_DIR}/testcases)
