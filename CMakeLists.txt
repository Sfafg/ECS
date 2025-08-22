cmake_minimum_required(VERSION 3.30.1)
project(ECS)

option(ECS_BUILD_TESTS "Build tests" FALSE)

set(TESTS_ROOT "${PROJECT_SOURCE_DIR}/tests")
set(SRC_ROOT "${PROJECT_SOURCE_DIR}/src")
set(CMAKE_CXX_STANDARD 23)

# Library
file(GLOB_RECURSE SRC "${SRC_ROOT}/*.cpp")
file(GLOB_RECURSE HEADER "${SRC_ROOT}/*.h")

add_library(ECS STATIC)
target_sources(ECS
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES ${HEADER}
)
target_include_directories(ECS PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

if(ECS_BUILD_TESTS)
    add_executable(ECSTest ${TESTS_ROOT}/Source.cpp)
    target_link_libraries(ECSTest PUBLIC ECS)

    add_executable(Test ${TESTS_ROOT}/Test.cpp)
    target_link_libraries(Test PUBLIC ECS)
endif()
