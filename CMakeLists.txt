cmake_minimum_required(VERSION 3.30.1)
project(ECS)
set(CMAKE_CXX_STANDARD 23)
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
set(CMAKE_EXE_LINKER_FLAGS "-static")

set(TESTS_ROOT "${PROJECT_SOURCE_DIR}/tests")
set(SRC_ROOT "${PROJECT_SOURCE_DIR}/src")

add_executable(ECSTest ${SRC_ROOT}/ECS.cpp ${SRC_ROOT}/PopbackArray.cpp ${TESTS_ROOT}/Source.cpp)
target_include_directories(ECSTest PUBLIC ${SRC_ROOT})

add_executable(Test ${SRC_ROOT}/ECS.cpp ${SRC_ROOT}/PopbackArray.cpp ${TESTS_ROOT}/Test.cpp)
target_include_directories(Test PUBLIC ${SRC_ROOT})

include(CTest)
add_test(NAME Test COMMAND Test)
