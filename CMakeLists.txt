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
    src/h_bitset.c

    src/core/semantic_analyzer.c

    src/core/ir_gen.c
    src/core/ir_lower.c
    src/core/ir_dumper.c

    src/opt/opt.c
    src/opt/register_allocator.c

    src/backend/codegen.c
    src/backend/x86_64/x86_64_linux.c
    # src/opt/opt.c
)

target_compile_options(hrsc PRIVATE -g)

target_include_directories(hrsc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
