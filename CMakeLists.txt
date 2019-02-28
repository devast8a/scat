cmake_minimum_required(VERSION 3.6)
project(scat)

set(CMAKE_CXX_STANDARD 17)

# Set default build type to release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    message("[scat] No build mode set, defaulting to Release")
endif()

# Dependencies

# main
add_executable(main src/main.cpp)
target_include_directories(main PRIVATE includes)
