cmake_minimum_required(VERSION 3.25)
project(image-downscaler)

set(CMAKE_CXX_STANDARD 23)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(image-downscaler main.cpp)
target_link_libraries(image-downscaler ${OpenCV_LIBS})
