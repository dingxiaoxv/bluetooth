cmake_minimum_required(VERSION 3.0.2)
project(bluekeyman)

set_directory_properties(PROPERTIES COMPILE_OPTIONS "-g")

include_directories(include)

add_executable(bluekeyman src/main.cpp src/bluebus.cpp)

target_link_libraries(bluekeyman systemd)