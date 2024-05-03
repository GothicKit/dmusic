cmake_minimum_required(VERSION 3.10)

add_library(dmusic-tsf STATIC TinySoundFont/tsf.c)
target_include_directories(dmusic-tsf PUBLIC TinySoundFont)
