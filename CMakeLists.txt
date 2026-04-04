cmake_minimum_required(VERSION 3.16)

project(HupeyaszihProgrammingLanguage LANGUAGES C)

set(CMAKE_C\_STANDARD 11)
set(CMAKE_C\_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(HupeyaszihProgrammingLanguage
    src/main.c
    src/lexer.c
)

target_include_directories(HupeyaszihProgrammingLanguage
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
