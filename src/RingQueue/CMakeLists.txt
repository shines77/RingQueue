
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_C_COMPILER gcc)

#
# Use sse, sse2 in gcc
# Reference: http://stackoverflow.com/questions/5217812/c-compilation-issue-with-emmintrin-h-on-linux-gcc
#
if (NOT MSVC_IDE)
    add_definitions("-D_GNU_SOURCE")

    add_definitions("-msse")
    add_definitions("-msse2")

    add_definitions("-D__MMX__")
    add_definitions("-D__SSE__")
    add_definitions("-D__SSE2__")
    add_definitions("-D__SSE3__")

    # add_definitions("-lpthread")
endif()

if (CMAKE_COMPILER_IS_GNUCC)
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lpthread")
    # set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -lpthread")
    # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -lpthread")
elseif (CMAKE_COMPILER_IS_GNUCXX)
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread")
    # set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -lpthread")
    # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -lpthread")
endif()

# set(SRC_RINGQUEUE_LIST main.cpp console.c dump_mem.c get_char.c sleep.c sys_timer.c ${SRC_RINGQUEUE_COLMAJOR_LIST} ${SRC_RINGQUEUE_ROWMAJOR_LIST})

file(GLOB_RECURSE SRC_RINGQUEUE_LIST "${PROJECT_SOURCE_DIR}/src/RingQueue/*.cpp" "${PROJECT_SOURCE_DIR}/src/RingQueue/*.c")

# aux_source_directory(. SRC_RINGQUEUE_LIST)
# aux_source_directory(./msvc SRC_RINGQUEUE_MSVC_LIST)

file(GLOB_RECURSE SRC_RINGQUEUE_INCLUDE_LIST "${PROJECT_SOURCE_DIR}/include/RingQueue/*.h")

# list(APPEND SRC_RINGQUEUE_LIST ${SRC_RINGQUEUE_MSVC_LIST} ${SRC_RINGQUEUE_INCLUDE_LIST})

# aux_source_directory(./colmajor SRC_RINGQUEUE_COLMAJOR_LIST)
# aux_source_directory(./rowmajor SRC_RINGQUEUE_ROWMAJOR_LIST)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/RingQueue)

# add_library(util STATIC ${SRC_LIST})
# add_library(libhello SHARED ${LIB_SRC})
# link_directories(${PROJECT_SOURCE_DIR}/lib)

message(STATUS "This is BINARY dir " ${PROJECT_BINARY_DIR})
message(STATUS "This is SOURCE dir " ${PROJECT_SOURCE_DIR})

# message(STATUS "This is SOURCE colmajor dir " ${SRC_RINGQUEUE_COLMAJOR_LIST})
# message(STATUS "This is SOURCE rowmajor dir " ${SRC_RINGQUEUE_ROWMAJOR_LIST})

# message(STATUS "This is SOURCE RingQueue dir " ${SRC_RINGQUEUE_LIST})

# message(STATUS "CMAKE_STANDARD_LIBRARIES = " ${CMAKE_STANDARD_LIBRARIES})

#
# cmake and lib pthread
# See: http://stackoverflow.com/questions/5395309/cmake-and-threads
#
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

find_package(Threads REQUIRED)
# find_package(Kernel32 REQUIRED)

if (CMAKE_USE_PTHREADS_INIT)
    set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-lpthread")
endif()

add_executable(RingQueue ${SRC_RINGQUEUE_LIST} ${SRC_RINGQUEUE_INCLUDE_LIST})
# add_executable(RingQueue ${SRC_RINGQUEUE_LIST} ${SRC_RINGQUEUE_COLMAJOR_LIST} ${SRC_RINGQUEUE_ROWMAJOR_LIST})

if (CMAKE_USE_PTHREADS_INIT)
    target_link_libraries(RingQueue ${CMAKE_THREAD_LIBS_INIT})
endif()

#if (MINGW OR CYGWIN OR MSVC_IDE)
    # msvcrt.lib
    target_link_libraries(RingQueue kernel32 user32 gdi32 comdlg32 shell32 uuid winmm)
#endif()

# target_link_libraries(hello util)

#
# See: http://yang3wei.github.io/blog/2013/02/15/cmake-xue-xi-bi-ji-1/
#
# See: http://blog.csdn.net/dbzhang800/article/details/6314073
#
# set_target_properties(libhello PROPERTIES OUTPUT_NAME "hello")
