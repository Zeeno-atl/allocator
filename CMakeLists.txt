cmake_minimum_required(VERSION 3.26)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_CURRENT_LIST_DIR}/build/${CMAKE_BUILD_TYPE})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/build/${CMAKE_BUILD_TYPE})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/out)

project(allocator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/W4)
    endif()
    set(CMAKE_LINK_WHAT_YOU_USE ON)
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

file(GLOB_RECURSE sources "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/modules/*.cpp") 

add_executable(${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PRIVATE "include")
target_sources(${PROJECT_NAME}
    PRIVATE ${sources}
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -stdlib=libc++)
    target_link_options(${PROJECT_NAME} PRIVATE -stdlib=libc++)
endif()

# static link for the release build
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(${PROJECT_NAME} PRIVATE)
else()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE -fno-omit-frame-pointer -fsanitize=address -fsanitize=leak)
        target_link_options(${PROJECT_NAME} PRIVATE -fno-omit-frame-pointer -fsanitize=address -fsanitize=leak)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${PROJECT_NAME} PRIVATE /fsanitize=address /analyze /analyze:external-)
        target_link_options(${PROJECT_NAME} PRIVATE /fsanitize=address /analyze /analyze:external-)
    endif()
endif()

find_package(range-v3 CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    range-v3::range-v3 
)
