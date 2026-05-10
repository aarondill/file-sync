cmake_minimum_required(VERSION 3.20)
project(file-sync CXX)

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
endif()

add_compile_definitions(DEBUG=$<CONFIG:Debug>)

if (NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  # Auto-create a .gitignore in the specified "build" directory.
  file(GENERATE OUTPUT ${PROJECT_BINARY_DIR}/.gitignore CONTENT "*")
endif ()

add_executable(main)

if (MSVC)
  message(SEND_ERROR "This project doesn't officially support MSVC. Your milage may vary. Pull requests are welcome.")
endif ()

if (NOT MSVC)
  target_compile_options(main PRIVATE
            -Wall -Wextra -Wpedantic -Wno-changes-meaning
            $<$<CONFIG:Debug>:-Werror -Wno-unused>
  )

  if (CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "Enabling address sanitizer and undefined behavior sanitizer.")
    # -fsanitize=undefined enabling the detection of undefined behavior such as integer overflows,
    # -fsanitize=address enabling the detection of memory problems such as buffer overflows and memory leaks
    target_compile_options(main PRIVATE -fsanitize=undefined,address)
    target_link_options(main PRIVATE -fsanitize=undefined,address)
  endif ()
endif ()


file(GLOB SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB HEADERS CONFIGURE_DEPENDS src/*.h)
target_sources(main PRIVATE ${SOURCES} ${HEADERS})
