cmake_minimum_required(VERSION 2.8)

project(qttp)

# C++11
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Create bin and lib, dir, and logs
make_directory(lib)
make_directory(bin)
make_directory(logs)

# Set output dirs
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# HTTP_PARSER library
#add_library(HTTPPARSER STATIC include/http_parser.o)
#SET_TARGET_PROPERTIES(HTTPPARSER PROPERTIES LINKER_LANGUAGE C)

# Search for 
include_directories(src include)
 
add_executable(car src/main.cpp)
target_link_libraries(car pthread wiringPi opencv)

# Testing
#find_package(GTest REQUIRED)
#include_directories(${GTEST_INCLUDE_DIRS})

#add_executable(run_tests test/main.cpp src/logging.cpp
#  test/connection_queue_test.cpp src/connection_queue.cpp
#  test/connection_pool_test.cpp src/connection_pool.cpp
#  test/connection_pool_test_threaded.cpp
#  test/connection_worker_test.cpp
#  test/qttp_test.cpp src/qttp.cpp
#  src/connection_handler_epoll.cpp
#  src/connection_worker.cpp
#  src/work_queue.cpp
#  src/job_handler.cpp)

#target_link_libraries(run_tests ${GTEST_LIBRARIES} pthread curlpp curl log4cpp HTTPPARSER)

#enable_testing()
#add_test(NAME UnitTests COMMAND run_tests)
