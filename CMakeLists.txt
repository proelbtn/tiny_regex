cmake_minimum_required(VERSION 3.9)
project(tiny_regex)

include_directories(./include /usr/local/include)

add_subdirectory(src)
add_subdirectory(test)

set(SRCS main.cpp)

add_executable(main ${SRCS})
target_link_libraries(main tiny_regex)
