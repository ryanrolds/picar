cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/modules )

list(APPEND CMAKE_MODULE_PATH /usr/local/lib/cmake)

add_compile_options(-std=c++11)
#link_directories(/usr/local/lib)

# Search for 
include_directories(include)
include_directories(raspicam-0.1.6/src)
include_directories(/usr/local/include/eigen3)
include_directories(/usr/local/include/tf)
include_directories(/usr/local/include/tf/bazel-genfiles)
include_directories(/usr/local/include/tf/third_party)

# Create bin, and logs
make_directory(bin)
make_directory(logs)

file(COPY share/visionary.net_cat_cascade_web_LBP.xml DESTINATION share/picar)

# Set output dirs
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Picar
project(picar)

add_definitions(-DCPU_ONLY)

#configure_file(share/v4/net.model share/net.model COPYONLY)
#configure_file(share/v4/net.weights share/net.weights COPYONLY)
#configure_file(share/v4/net.avg share/net.avg COPYONLY)

find_package(OpenCV REQUIRED)
find_package(raspicam REQUIRED)
#find_package(Caffe2 REQUIRED)
find_package(CUDA QUIET REQUIRED)
find_package (Eigen3 3.3 REQUIRED NO_MODULE)

cuda_add_executable(picar-brain src/brain.cpp)
target_link_libraries(picar-brain pthread protoc cudnn tensorflow_framework tensorflow_cc boost_system glog Eigen3::Eigen ${OpenCV_LIBS})

cuda_add_executable(tf_test src/tf_test.cpp)
target_link_libraries(tf_test protoc cudnn tensorflow_framework tensorflow_cc boost_system glog Eigen3::Eigen ${OpenCV_LIBS}) 

add_executable(picar-car src/car.cpp src/control.cpp src/senses.cpp)
target_link_libraries(picar-car pthread wiringPi ${raspicam_LIBS})

add_executable(lidar-test src/lidar.cpp)
target_link_libraries(lidar-test pthread bcm2835)

# Old, switching to Cafee - here for reference
#find_package(OpenCV REQUIRED)

# Old, pre car+brain refactor - here for reference
#add_executable(picar src/main.cpp src/detect.cpp src/control.cpp src/senses.cpp)
#target_link_libraries(picar pthread wiringPi ${OpenCV_LIBS} ${raspicam_CV_LIBS})

install(TARGETS picar-brain picar-car DESTINATION bin)
install(PROGRAMS scripts/picar-car.pre DESTINATION bin)
