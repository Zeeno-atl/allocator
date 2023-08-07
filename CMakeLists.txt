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
        add_compile_options(-Wall -Wextra -pedantic)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/W4)
    endif()
    set(CMAKE_LINK_WHAT_YOU_USE ON)
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# clang-tidy
find_program(CLANG_TIDY NAMES "clang-tidy-17" "clang-tidy-16" "clang-tidy-15" "clang-tidy-14" "clang-tidy-13" "clang-tidy" REQUIRED)
if(NOT CLANG_TIDY_NOT_FOUND)
    message(STATUS "Clang-tidy found at ${CLANG_TIDY}")
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY}
        --format-style=file
        --checks=-*,modernize-*,bugprone-*,clang-analyzer-*,cppcoreguidelines-*,performance-*,portability-*,readibility-*,-cppcoreguidelines-owning-memory,-cppcoreguidelines-pro-type-reinterpret-cast,-cppcoreguidelines-pro-bounds-pointer-arithmetic,-cppcoreguidelines-no-malloc,-cppcoreguidelines-avoid-magic-numbers,-bugprone-sizeof-expression,-performance-avoid-endl
        --header-filter=allocator\.hpp
    )
endif()

# cppcheck
find_program(CPPCHECK NAMES "cppcheck" REQUIRED)
if(NOT CPPCHECK_NOT_FOUND)
    message(STATUS "cppcheck found at ${CPPCHECK}")
    set(CMAKE_CXX_CPPCHECK ${CPPCHECK}
        --enable=all
        -D__cppcheck__
    )
endif()

file(GLOB_RECURSE sources "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/modules/*.cpp") 

add_executable(${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PRIVATE "include")
target_sources(${PROJECT_NAME}
    PRIVATE ${sources}
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -stdlib=libstdc++)
    target_link_options(${PROJECT_NAME} PRIVATE -stdlib=libstdc++)
endif()

# static link for the release build
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(${PROJECT_NAME} PRIVATE)
else()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # -fsanitize=address -fsanitize=leak
    # -fsanitize=memory
    # -fsanitize=thread
        target_compile_options(${PROJECT_NAME} PRIVATE -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=address)
        target_link_options(${PROJECT_NAME} PRIVATE -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=address)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${PROJECT_NAME} PRIVATE /fsanitize=address /analyze /analyze:external-)
        target_link_options(${PROJECT_NAME} PRIVATE /fsanitize=address /analyze /analyze:external-)
    endif()
endif()

find_package(mio CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    mio::mio
)
