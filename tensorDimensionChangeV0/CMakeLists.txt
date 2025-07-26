cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(my_project)

set(CMAKE_PREFIX_PATH "/Users/shizhefu0/Desktop/tool/libtorch")  # ← update this

find_package(Torch REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main "${TORCH_LIBRARIES}")
set_property(TARGET main PROPERTY CXX_STANDARD 17)
