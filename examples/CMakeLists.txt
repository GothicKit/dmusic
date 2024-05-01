cmake_minimum_required(VERSION 3.10)

add_executable(play-segment play-segment.c)
target_link_libraries(play-segment PRIVATE dmusic m)
