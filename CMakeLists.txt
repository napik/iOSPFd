cmake_minimum_required(VERSION 3.16)
project(iOSPFd)

set(CMAKE_CXX_STANDARD 14)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(iOSPFd main.cpp src/ospf.cpp)

target_link_libraries(iOSPFd PRIVATE Threads::Threads)