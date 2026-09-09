cmake_minimum_required(VERSION 3.10)
project(ThreadPool LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
endif()

# C++03
add_executable(threadpool_cpp03 cpp03/main.cpp)
target_include_directories(threadpool_cpp03 PRIVATE cpp03 common)
if(UNIX AND NOT APPLE)
    target_link_libraries(threadpool_cpp03 Threads::Threads)
endif()

# C++11
add_executable(threadpool_cpp11 cpp11/main.cpp)
target_include_directories(threadpool_cpp11 PRIVATE cpp11)
target_compile_features(threadpool_cpp11 PRIVATE cxx_std_11)
if(UNIX AND NOT APPLE)
    target_link_libraries(threadpool_cpp11 Threads::Threads)
endif()

# C++17
add_executable(threadpool_cpp17 cpp17/main.cpp)
target_include_directories(threadpool_cpp17 PRIVATE cpp17)
target_compile_features(threadpool_cpp17 PRIVATE cxx_std_17)
if(UNIX AND NOT APPLE)
    target_link_libraries(threadpool_cpp17 Threads::Threads)
endif()

# Pure C
add_executable(threadpool_c c_threadpool/main.c c_threadpool/thread_pool.c)
target_include_directories(threadpool_c PRIVATE c_threadpool common)
target_compile_features(threadpool_c PRIVATE c_std_11)
if(UNIX AND NOT APPLE)
    target_link_libraries(threadpool_c Threads::Threads)
endif()

# ==================== Benchmarks ====================

# Benchmark: Pure C
add_executable(bench_c c_threadpool/bench.c c_threadpool/thread_pool.c)
target_include_directories(bench_c PRIVATE c_threadpool common)
if(UNIX AND NOT APPLE)
    target_link_libraries(bench_c Threads::Threads)
endif()

# Benchmark: C++03
add_executable(bench_cpp03 cpp03/bench.cpp)
target_include_directories(bench_cpp03 PRIVATE cpp03 common)
if(UNIX AND NOT APPLE)
    target_link_libraries(bench_cpp03 Threads::Threads)
endif()

# Benchmark: C++11
add_executable(bench_cpp11 cpp11/bench.cpp)
target_include_directories(bench_cpp11 PRIVATE cpp11)
target_compile_features(bench_cpp11 PRIVATE cxx_std_11)
if(UNIX AND NOT APPLE)
    target_link_libraries(bench_cpp11 Threads::Threads)
endif()

# Benchmark: C++17
add_executable(bench_cpp17 cpp17/bench.cpp)
target_include_directories(bench_cpp17 PRIVATE cpp17)
target_compile_features(bench_cpp17 PRIVATE cxx_std_17)
if(UNIX AND NOT APPLE)
    target_link_libraries(bench_cpp17 Threads::Threads)
endif()
