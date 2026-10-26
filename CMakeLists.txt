cmake_minimum_required(VERSION 3.16)

project(piggywrite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(piggywrite_core STATIC
    src/editor.cpp
)
target_include_directories(piggywrite_core PUBLIC src)

add_executable(piggywrite
    src/main.cpp
    src/terminal.cpp
)
target_link_libraries(piggywrite PRIVATE piggywrite_core)
if(WIN32)
    target_link_libraries(piggywrite PRIVATE shell32)
endif()
if(MINGW)
    # Keep the produced executable runnable without the MinGW runtime DLLs.
    target_link_options(piggywrite PRIVATE -static-libgcc -static-libstdc++)
endif()

if(MSVC)
    target_compile_options(piggywrite_core PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(piggywrite PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(piggywrite_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(piggywrite PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(piggywrite PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

enable_testing()

add_executable(piggywrite_tests tests/test_editor.cpp)
target_link_libraries(piggywrite_tests PRIVATE piggywrite_core)
if(MSVC)
    target_compile_options(piggywrite_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(piggywrite_tests PRIVATE -Wall -Wextra -Wpedantic
        -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

add_test(NAME piggywrite_tests COMMAND piggywrite_tests)
