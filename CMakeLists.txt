cmake_minimum_required(VERSION 3.10)
project(dmusic LANGUAGES C VERSION 0.0.1)

set(CMAKE_C_STANDARD 11)

option(DM_ENABLE_ASAN "DirectMusic: Enable sanitizers in debug builds." ON)
option(DM_BUILD_EXAMPLES "DirectMusic: Build the examples." OFF)

add_subdirectory(vendor)

list(APPEND _DM_SOURCE
        src/util/Array.h
        src/util/Tsf.c

        src/io/Band.c
        src/io/Common.c
        src/io/Dls.c
        src/io/Segment.c
        src/io/Style.c

        src/thread/Thread.h
        src/thread/Thread.Posix.c
        src/thread/Thread.Win32.c

        src/_Dls.h
        src/_Internal.h
        src/_Riff.h
        src/Array.c
        src/Common.c
        src/Composer.c
        src/Dls.c
        src/Band.c
        src/Loader.c
        src/Logger.c
        src/Memory.c
        src/Message.c
        src/Performance.c
        src/Riff.c
        src/Rng.c
        src/Segment.c
        src/Style.c
        src/Synth.c
)

include(support/BuildSupport.cmake)
bs_select_cflags(${DM_ENABLE_ASAN} _DM_COMPILE_FLAGS _DM_LINK_FLAGS)

add_library(dmusic SHARED)
target_sources(dmusic PRIVATE ${_DM_SOURCE})
target_include_directories(dmusic PUBLIC include)
target_include_directories(dmusic PRIVATE src)
target_compile_definitions(dmusic PRIVATE DM_BUILD=1)
target_compile_options(dmusic PRIVATE ${_DM_COMPILE_FLAGS})
target_link_options(dmusic PRIVATE ${_DM_LINK_FLAGS})
target_link_libraries(dmusic PRIVATE tsf)

if (MINGW)
    target_compile_definitions(dmusic PRIVATE _POSIX_C_SOURCE=1)
endif ()

set_target_properties(dmusic PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN 1
        DEBUG_POSTFIX "d"
        VERSION ${PROJECT_VERSION}
)

install(TARGETS dmusic ARCHIVE LIBRARY RUNTIME)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include" TYPE INCLUDE)

if (DM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
