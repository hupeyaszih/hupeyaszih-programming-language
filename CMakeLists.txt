cmake_minimum_required(VERSION 3.16)

project(hrsc LANGUAGES C)

set(CMAKE_C\_STANDARD 11)
set(CMAKE_C\_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(hrsc
    src/core/main.c
    src/core/lexer.c
    src/core/parser.c
    src/core/hrs_file_io.c
    src/core/symbol_table.c
    src/h_vector.c
    src/core/ir_gen.c
    src/core/ir_lower.c
    src/core/ir_dumper.c
    src/opt/opt.c
    src/backend/x86_64/codegen_x86_64.c
)

target_include_directories(hrsc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
