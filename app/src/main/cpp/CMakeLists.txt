cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Werror")

add_library( libnoizer # The name of the library.
             SHARED # Shared library.
             libnoizer.cpp
             )

target_link_libraries(libnoizer
                      android
                      log
                      OpenSLES)
