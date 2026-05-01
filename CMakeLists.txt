cmake_minimum_required(VERSION 3.16)

project(hrsc LANGUAGES C)

set(CMAKE_C\_STANDARD 11)
set(CMAKE_C\_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(hrsc
    src/main.c
    src/lexer.c
    src/parser.c
    src/hrs_file_io.c
    src/symbol_table.c
    src/codegen.c
)

target_include_directories(hrsc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
