# HUPEYASZIH PROGRAMMING LANGUAGE (.hrs)

---

> [!WARNING]
> **Project Status:** The language is still under development and currently only supports x86_64 linux!

---

## Table of Contents
* [What it is](#introduction)
* [Roadmap](#roadmap)
* [Philosophy](#philosophy)
* [Trade-offs](#tradeoffs)
* [Syntax](#syntax)
* [Key Features](#features)
* [Compiler Pipeline](#pipeline)
* [Building from Source](#compiling)
* [Usage](#usage)

---

## <a id="introduction"></a>What it is
**Hupeyaszih** is a low-level programming language built for absolute control. The compiler includes **no external dependencies like LLVM**—everything is handwritten in C from the ground up.
To force developers into writing branchless, cache-friendly, and high-performance code, **Hupeyaszih completely eliminates standard branching structures (`if`, `else`, `switch`)**.

* Conditional Flow: Since there are no if statements, flow control is handled via custom loop structure.
```
loop {
     ...
     condition; // if true, branches to continue block, else branches to return block
} return {
     ...
     return_value;
} continue{
     ...
}
```

* Raw Pointer Arithmetic: ptr + 1 always increments the address by exactly 1 byte. Use sizeof and alignof for manual scaling.

## <a id="roadmap"></a>Roadmap
### Done:
* variable support
* functions
* pointers
* Inline assembly support
* Loop
* sizeof/alignof/typeof/stof, note: stof(variable) means "sizeof(typeof(variable))"
* shadowing
* custom SSA-IR
* x86_64 linux backend/codegen and build target system

### Planned:
* Float support
* Structs and packed structs
* optimization passes
* "select" keyword to use cmov etc.
* import system
* standard library (StdLib)
* Self-hosting (Rewriting the compiler in .hrs)

## <a id="philosophy"></a>The Philosophy
Hupeyaszih aims to empower developers to create branchless, high-performance software with zero implicit overhead. No hidden branches, no implicit overhead. What you see is what the CPU executes.

## <a id="tradeoffs"></a>Trade-offs
* Still under development
* No import/include system yet, making it difficult to split projects into multiple files or build modular libraries.
* Unconventional control flow requires a mindset shift from traditional programming languages.
* Designed specifically for branchless programming, making it ideal for low-latency, high-performance domains like game engines, rendering pipelines, and audio processing.
* Zero bloat: Custom IR and handwritten backend give you complete visibility over code generation.

## <a id="syntax"></a>Syntax at a Glance
```hrs
fn fibonacci(n: int32) : int32 {
    var result: int32 = n;
    
    loop {
        n > 1;
    } return {
        result;
    } continue {
        result = fibonacci(n - 1) + fibonacci(n - 2);
        n = 0;
    }
}

fn main() : int32 {
    var n:int32 = 10;

    fibonacci(n);
}
```

## <a id="features"></a>Key Features
* Enforces to use less branches
* Low-level language
* Written from scratch in C
* Handwritten, doesn't use any library such as LLVM
* Native support for upcoming features like approximate loops and resilient blocks.
* Custom IR and loop structure

## <a id="pipeline"></a>Compiler Pipeline
```text

[Source Code (.hrs)] 
        │
        ▼
     [ Lexer ]
        │
        ▼
     [ Parser ] --> (AST)
        │
        ▼
[ Semantic Analyzer ] --> (Type Checking etc.)
        |
        ▼
[ Code Generator ] --> [ Custom IR ] --> [ Optimization Passes ] --> [ Custom Backend/Codegen ] --> [ Native Executable ]
```

## <a id="compiling"></a>Building from Source
### Prerequisites
* C compiler
* CMake

### Build Steps
```
git clone https://github.com/hupeyaszih/hupeyaszih-programming-language.git
cd hupeyaszih-programming-language
mkdir build && cd build
cmake ..
make
```

## <a id="usage"></a>Usage
To compile a Hupeyaszih (.hrs) file:
```
./hrsc <file_path>.hrs -o <file_path>.s --run
```

Example:
```
./hrsc ../example/example_00.hrs -o ../out/example_00.s --run
```
