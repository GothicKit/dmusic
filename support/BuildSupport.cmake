## Compute the compile and link play_mode_flags for the current compiler.
##
## Args:
##   SANITIZERS(bool): Whether to enable sanitizers or not.
##
## Returns:
##   COMPILE(string): A string containing the compiler play_mode_flags, separated by spaces
##   LINK(string): A string containing the linker play_mode_flags, separated by spaces
function(bs_select_cflags SANITIZERS COMPILE LINK)
    if (MSVC)
        bs_internal_select_cflags_msvc(${SANITIZERS} _INTERNAL_COMPILE_FLAGS _INTERNAL_LINK_FLAGS)
    elseif(CMAKE_C_COMPILER_ID MATCHES "^Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "^Clang")
        bs_internal_select_cflags_clang(${SANITIZERS} _INTERNAL_COMPILE_FLAGS _INTERNAL_LINK_FLAGS)
    elseif(CMAKE_C_COMPILER_ID MATCHES "^GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "^GCC")
        bs_internal_select_cflags_gcc(${SANITIZERS} _INTERNAL_COMPILE_FLAGS _INTERNAL_LINK_FLAGS)
    else()
        message(WARNING "DirectMusic: Unknown compiler: Won't set CFLAGS!")
    endif()

    if (MINGW)
        list(APPEND _INTERNAL_COMPILE_FLAGS "-static")
        list(APPEND _INTERNAL_LINK_FLAGS "-static")
    endif()

    # return _INTERNAL_COMPILE_FLAGS, _INTERNAL_LINK_FLAGS;
    set(${COMPILE} ${_INTERNAL_COMPILE_FLAGS} PARENT_SCOPE)
    set(${LINK} ${_INTERNAL_LINK_FLAGS} PARENT_SCOPE)
    return()
endfunction()

## Compute the compile and link play_mode_flags for MSVC
##
## Args:
##   SANITIZERS(bool): Whether to enable sanitizers or not.
##
## Returns:
##   COMPILE(list): A list containing the compiler play_mode_flags
##   LINK(list): A list containing the linker play_mode_flags
function(bs_internal_select_cflags_msvc SANITIZERS COMPILE LINK)
    list(APPEND _INTERNAL_COMPILE_FLAGS "/Wall")

    if (CMAKE_BUILD_TYPE MATCHES "^Debug" AND ${SANITIZERS})
        list(APPEND _INTERNAL_COMPILE_FLAGS "/fsanitize=address")
    endif()

    # return _INTERNAL_COMPILE_FLAGS, _INTERNAL_LINK_FLAGS;
    set(${COMPILE} ${_INTERNAL_COMPILE_FLAGS} PARENT_SCOPE)
    set(${LINK} ${_INTERNAL_LINK_FLAGS} PARENT_SCOPE)
    return()
endfunction()

## Compute the compile and link play_mode_flags for GCC
##
## Args:
##   SANITIZERS(bool): Whether to enable sanitizers or not.
##
## Returns:
##   COMPILE(list): A list containing the compiler play_mode_flags
##   LINK(list): A list containing the linker play_mode_flags
function(bs_internal_select_cflags_gcc SANITIZERS COMPILE LINK)
    # "Who cares about ISO C anyway?"
    list(APPEND _INTERNAL_FLAGS "-Wall" "-Wextra" "-Werror" "-Wshadow" "-fPIC" "-flto" "-Wno-cast-function-type")

    if (CMAKE_BUILD_TYPE MATCHES "^Debug" AND ${SANITIZERS} AND NOT WIN32)
        list(APPEND _INTERNAL_FLAGS "-fsanitize=address" "-fsanitize=undefined" "-fsanitize=leak")
    endif()

    # return _INTERNAL_FLAGS, _INTERNAL_FLAGS;
    set(${COMPILE} ${_INTERNAL_FLAGS} PARENT_SCOPE)
    set(${LINK} ${_INTERNAL_FLAGS} PARENT_SCOPE)
    return()
endfunction()

## Compute the compile and link play_mode_flags for Clang
##
## Args:
##   SANITIZERS(bool): Whether to enable sanitizers or not.
##
## Returns:
##   COMPILE(list): A list containing the compiler play_mode_flags
##   LINK(list): A list containing the linker play_mode_flags
function(bs_internal_select_cflags_clang SANITIZERS COMPILE LINK)
    bs_internal_select_cflags_gcc(${SANITIZERS} _INTERNAL_COMPILE_FLAGS _INTERNAL_LINK_FLAGS)

    if (CMAKE_BUILD_TYPE MATCHES "^Debug")
        list(APPEND _INTERNAL_COMPILE_FLAGS "-fstandalone-debug")
        list(APPEND _INTERNAL_LINK_FLAGS "-fstandalone-debug")
    endif()

    # return _INTERNAL_COMPILE_FLAGS, _INTERNAL_LINK_FLAGS;
    set(${COMPILE} ${_INTERNAL_COMPILE_FLAGS} PARENT_SCOPE)
    set(${LINK} ${_INTERNAL_LINK_FLAGS} PARENT_SCOPE)
    return()
endfunction()
