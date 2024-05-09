cmake_minimum_required(VERSION 3.10)

add_library(dmusic-tsf STATIC TinySoundFont/tsf.c)
target_include_directories(dmusic-tsf PUBLIC TinySoundFont)

if (NOT MSVC)
    target_compile_options(dmusic-tsf PRIVATE "-fPIC")
    target_link_options(dmusic-tsf PRIVATE "-fPIC")
endif ()
