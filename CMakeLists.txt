cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH "/usr/local/lib/cmake")

add_compile_options(-std=c++11)
link_directories(/usr/local/lib)
# Search for 
include_directories(include)

# Create bin, and logs
make_directory(bin)
make_directory(logs)

# Set output dirs
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Car
project(car)

find_package(raspicam REQUIRED)
find_package(OpenCV REQUIRED)
 
add_executable(car src/main.cpp src/detect.cpp src/control.cpp)
target_link_libraries(car pthread wiringPi ${OpenCV_LIBS} ${raspicam_LIBS} ${raspicam_cv_LIBS})

