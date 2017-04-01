cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/modules )

list(APPEND CMAKE_MODULE_PATH /usr/local/lib/cmake)

add_compile_options(-std=c++11)
link_directories(/usr/local/lib)
# Search for 
include_directories(include)

# Create bin, and logs
make_directory(bin)
make_directory(logs)

file(COPY share/visionary.net_cat_cascade_web_LBP.xml DESTINATION bin/)

# Set output dirs
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Picar
project(picar)

find_package(raspicam REQUIRED)
find_package(OpenCV REQUIRED)
 
add_executable(picar src/main.cpp src/detect.cpp src/control.cpp src/senses.cpp)
target_link_libraries(picar pthread wiringPi ${OpenCV_LIBS} ${raspicam_CV_LIBS})
