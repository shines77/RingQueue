cmake_minimum_required(VERSION 2.8.8)

set(JLANG_CMAKE_SHOW_DETAIL 0)
set(JLANG_CMAKE_SHOW_MSVC_DETAIL 0)
set(JLANG_CMAKE_SHOW_DEBUG 1)
set(JLANG_CMAKE_SHOW_INFO 1)

# Add path for custom modules
set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules"
)

message(STATUS "CMAKE_SOURCE_DIR = " ${CMAKE_SOURCE_DIR})
message(STATUS "CMAKE_BINARY_DIR = " ${CMAKE_BINARY_DIR})
if (JLANG_CMAKE_SHOW_DETAIL)
    message(STATUS "CMAKE_CURRENT_SOURCE_DIR = " ${CMAKE_CURRENT_SOURCE_DIR})
    message(STATUS "CMAKE_CURRENT_BINARY_DIR = " ${CMAKE_CURRENT_BINARY_DIR})
endif()
message(STATUS "")

if (JLANG_CMAKE_SHOW_DETAIL)
    message(STATUS "Now create a empty project.")
    message(STATUS "")
endif()

# Create a empty project to get the project's variants value
project("")
message(STATUS "")

if (JLANG_CMAKE_SHOW_DETAIL)
    message(STATUS "Create the empty project done.")
    message(STATUS "")
endif()

# If we are not building as a part of JLVM, build JLang as an
# standalone project, using JLVM as an external library:
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # set(CMAKE_C_COMPILER g++)

    if (JLANG_CMAKE_SHOW_DETAIL)
        message(STATUS "Start to detect the compiler environment ...")
        message(STATUS "")
    endif()

    include(GetCompilerToolset)

    # Get the Compiler Toolset
    set(COMPILER_TOOLSET)
    GetCompilerToolset(COMPILER_TOOLSET "cxx")
    message(STATUS "COMPILER_TOOLSET = ${COMPILER_TOOLSET}")
    message(STATUS "")

    include(DisplayComplierEnv)

    # Check Compiler Environment Values
    DisplayCompilerEnvironment()

    if (JLANG_CMAKE_SHOW_DETAIL)
        message(STATUS "Create the real project files.")
    endif()
    if ((NOT MSVC) OR (COMPILER_TOOLSET STREQUAL ""))
        # message(STATUS "The real project filename is \"RingQueue\".")
        project("RingQueue")
    else()
        # message(STATUS "The real project filename is \"RingQueue_${COMPILER_TOOLSET}\".")
        project("RingQueue_${COMPILER_TOOLSET}")
    endif()
    if (JLANG_CMAKE_SHOW_DETAIL)
        message(STATUS "Create the real project files done.")
    endif()
    message(STATUS "")

    # include_directories(${PROJECT_SOURCE_DIR}/include)
    # add_library(util STATIC ${SRC_LIST})
    # add_library(libhello SHARED ${LIB_SRC})
    # link_directories(${PROJECT_SOURCE_DIR}/lib)

    message(STATUS "PROJECT_SOURCE_DIR = " ${PROJECT_SOURCE_DIR})
    message(STATUS "PROJECT_BINARY_DIR = " ${PROJECT_BINARY_DIR})
    message(STATUS "")

    set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
    set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

    message(STATUS "EXECUTABLE_OUTPUT_PATH = " ${EXECUTABLE_OUTPUT_PATH})
    message(STATUS "LIBRARY_OUTPUT_PATH    = " ${LIBRARY_OUTPUT_PATH})
    message(STATUS "")

    set(TOOLS_BINARY_DIR ${PROJECT_BINARY_DIR}/bin)
    set(LIBRARY_DIR      ${PROJECT_BINARY_DIR}/lib)
    set(INCLUDE_DIR      ${PROJECT_SOURCE_DIR}/src)
    set(JLVM_OBJ_ROOT    ${PROJECT_BINARY_DIR})
    set(MAIN_SRC_DIR     ${PROJECT_BINARY_DIR}/src)

    set(JLVM_INCLUDE_DIR ${JLVM_BINARY_DIR}/src)

    set(JLVM_TOOLS_BINARY_DIR ${TOOLS_BINARY_DIR} CACHE PATH "Path to jlvm/bin")
    set(JLVM_LIBRARY_DIR ${LIBRARY_DIR} CACHE PATH "Path to jlvm/lib")
    set(JLVM_MAIN_INCLUDE_DIR ${INCLUDE_DIR} CACHE PATH "Path to jlvm/include")
    set(JLVM_BINARY_DIR ${JLVM_OBJ_ROOT} CACHE PATH "Path to JLVM build tree")
    set(JLVM_MAIN_SRC_DIR ${MAIN_SRC_DIR} CACHE PATH "Path to JLVM source tree")

    if (EXISTS ${JLVM_INCLUDE_DIR})
        include_directories("${JLVM_INCLUDE_DIR}" "${JLVM_BINARY_DIR}/include" "${JLVM_MAIN_INCLUDE_DIR}")
    else()
        include_directories("${JLVM_BINARY_DIR}/include" "${JLVM_MAIN_INCLUDE_DIR}")
    endif()
    link_directories("${JLVM_LIBRARY_DIR}")

    #
    # In CMake, how do I work around the Debug and Release directories Visual Studio 2010 tries to add?
    #
    # See: http://stackoverflow.com/questions/7747857/in-cmake-how-do-i-work-around-the-debug-and-release-directories-visual-studio-2
    #

    # First for the generic no-config case (e.g. with mingw)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

    # Second, for multi-config builds (e.g. msvc)
    foreach (OUTPUT_CONFIG ${CMAKE_CONFIGURATION_TYPES})
        #string(TOUPPER ${OUTPUT_CONFIG} OUTPUT_CONFIG)
        string(TOLOWER ${OUTPUT_CONFIG} OUTPUT_CONFIG)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUT_CONFIG} ${CMAKE_BINARY_DIR}/bin/${OUTPUT_CONFIG})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUT_CONFIG} ${CMAKE_BINARY_DIR}/lib/${OUTPUT_CONFIG})
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUT_CONFIG} ${CMAKE_BINARY_DIR}/lib/${OUTPUT_CONFIG})
    endforeach(OUTPUT_CONFIG CMAKE_CONFIGURATION_TYPES)
endif()

set(JLANG_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(JLANG_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE)
    message(FATAL_ERROR "In-source builds are not allowed. CMake would overwrite "
            "the makefiles distributed with LLVM. Please create a directory and run cmake "
            "from there, passing the path to this source directory as the last argument. "
            "This process created the file `CMakeCache.txt' and the directory "
            "`CMakeFiles'. Please delete them.")
endif()

# Add appropriate flags for GCC
if (JLVM_COMPILER_IS_GCC_COMPATIBLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-common -Woverloaded-virtual -Wcast-qual -fno-strict-aliasing")

    # Enable -pedantic for Clang even if it's not enabled for JLVM.
    if (NOT JLVM_ENABLE_PEDANTIC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wno-long-long")
    endif ()

    check_cxx_compiler_flag("-Werror -Wnested-anon-types" CXX_SUPPORTS_NO_NESTED_ANON_TYPES_FLAG)
    if (CXX_SUPPORTS_NO_NESTED_ANON_TYPES_FLAG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-nested-anon-types" )
    endif()
endif ()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(BEFORE
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(src)
