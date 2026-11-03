cmake_minimum_required(VERSION 3.20)
project(ads_recover_the_design LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(recover
    code/src/main.cpp
    code/src/solver.cpp
)
target_include_directories(recover PRIVATE code/src)

# 固定 -O2：报告第 3、4 章的运行时间都是在这个优化级别下测的，换级别数字不可比。
target_compile_options(recover PRIVATE -O2 -Wall -Wextra)
