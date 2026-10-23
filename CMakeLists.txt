cmake_minimum_required(VERSION 3.0.0)
set (CMAKE_CXX_STANDARD 20)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g3")

project(plain_bplustree VERSION 0.1.0)

include(CTest)
enable_testing()
find_package(Threads REQUIRED)
find_package(Boost COMPONENTS serialization REQUIRED)

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

include(FetchContent)
FetchContent_Declare(
    perf-cpp-external
    GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
    GIT_TAG "v0.10.0"
)
FetchContent_MakeAvailable(perf-cpp-external)
include_directories(${perf-cpp-external_SOURCE_DIR}/include)

add_executable(plain_bplustree main.cpp)
target_link_libraries(plain_bplustree Threads::Threads perf-cpp ${Boost_LIBRARIES}) 

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
