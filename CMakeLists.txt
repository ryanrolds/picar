cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH "/usr/local/lib/cmake")
link_directories(/usr/local/lib)

project(car)

# C++11
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(raspicam REQUIRED)
find_package(OpenCV REQUIRED)

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
target_link_libraries(car pthread wiringPi ${OpenCV_LIBS} ${raspicam_LIBS})
