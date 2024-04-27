cmake_minimum_required(VERSION 3.10)

add_library(tsf TinySoundFont/tsf.c)
target_include_directories(tsf PUBLIC TinySoundFont)
target_compile_options(tsf PRIVATE "-fPIC")
target_link_options(tsf PRIVATE "-fPIC")
