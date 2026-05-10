cmake_minimum_required(VERSION 3.20)
project(file-sync CXX)

set(ignoreMe "${CMAKE_C_COMPILER}")

if (NOT CMAKE_BUILD_TYPE)
    message(WARNING "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Create symlink to compile_commands.json for IDE to pick it up
# Don't create in CLion, where its unneeded and there's multiple configs at once
if (PROJECT_IS_TOP_LEVEL AND UNIX AND NOT "$ENV{CLION_IDE}")
    if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
                ${CMAKE_BINARY_DIR}/compile_commands.json
                ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
        )
    endif ()
endif ()

add_compile_definitions(DEBUG=$<CONFIG:Debug>)

if (NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    # Auto-create a .gitignore in the specified "build" directory.
    file(GENERATE OUTPUT ${PROJECT_BINARY_DIR}/.gitignore CONTENT "*")
endif ()


file(GLOB_RECURSE LIB_SOURCES src/lib/*.cpp)
file(GLOB_RECURSE LIB_HEADERS src/lib/*.h)
add_library(lib ${LIB_SOURCES} ${LIB_HEADERS})

file(GLOB EXECS src/*.cpp)
foreach (src ${EXECS})
    # Cut off the file extension and directory path
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_compile_options(${name} PRIVATE
            -Wall -Wextra -Wpedantic -Wno-changes-meaning
            $<$<CONFIG:Debug>:-Werror -Wno-unused>
    )

    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        message(STATUS "Enabling address sanitizer and undefined behavior sanitizer.")
        # -fsanitize=undefined enabling the detection of undefined behavior such as integer overflows,
        # -fsanitize=address enabling the detection of memory problems such as buffer overflows and memory leaks
        target_compile_options(${name} PRIVATE -fsanitize=undefined,address)
        target_link_options(${name} PRIVATE -fsanitize=undefined,address)
    endif ()
    target_link_libraries(${name} lib)
endforeach (src ${EXECS})

