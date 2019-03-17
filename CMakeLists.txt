cmake_minimum_required(VERSION 3.6)
project(scat)

set(CMAKE_CXX_STANDARD 17)

# Set default build type to release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    message("[scat] No build mode set, defaulting to Release")
endif()

# Dependencies
add_subdirectory(vendor/catch2)


# main
add_executable(main src/main.cpp)
target_include_directories(main PRIVATE includes)

add_executable(tests tests/test-main.cpp tests/constant.cpp)
target_link_libraries(tests catch2)
target_include_directories(tests PRIVATE includes)

add_executable(L3-rattle src/L3-rattle.cpp)
