## NOTICE:
* Currently, this compiler targets Linux x86_64 systems only!
* This project is under active development and is not yet stable for production use.

# Hupeyaszih Programming Language (.hrs)
A low-level programming language built for absolute control. This compiler doesn't include LLVM etc. Everything is handwritten in C.
Hupeyaszih Programming Language **does not have** "if/else/switch"** etc. because the language wants programmer to write branchless code. The language has branch in only loops (loop/break/continue).
The language eliminates standard branching structures (if, else, switch) to enforce branchless programming style.

* **Conditional Flow:** Since there are no if statements, flow control is handled via conditional loop, break, and continue.
```hrs 
loop{
    continue(condition) { ... } // Executes the block and jumps to the next iteration **only if** the condition is true.

    break(condition) { ... }    // Executes the block and exits the loop **only if** the condition is true.
}
```
* **Raw Pointer Arithmetic:** `ptr + 1` always increments the address by exactly **1 byte**. Use `sizeof` and `alignof` for manual scaling.

# Example code

```hrs
// Hupeyaszih Programming Language (.hrs)

fn main(): int32 {
    var test_pointer_val: int32 = 10;
    var test_pointer: ptr = &test_pointer_val;
    *test_pointer = (*test_pointer) + 1*alignof(int32);
    {
        var start_index: int32 = 65; // A = 65
        var index: int32 = start_index-1;
        var create_new_line:int32 = 1;
            loop {
                index = index + 1;

                var is_invalid: int32 = (index > 90) * (index < 97);
                continue(is_invalid) { // You can directly use "continue(index == 91) {index = index + 5; print_char(10);}"
                    print_char(10 * create_new_line);
                    create_new_line = 0;
                }

                print_char(index);
                break(index == start_index + 57) {
                    print_char(10);
                }
            }

    }

    var total: int32 = 0;
    var x: int32 = 1;

    fn calculate_bonus(val: int32): int32 {
        var bonus: int32 = 0;
        bonus = (val > 4) * 10;
        bonus;
    }


    loop {
        continue(x - ((x / 3) * 3) == 0) {
            x = x + 1;
        }

        total = total + (get_weight(x) + calculate_bonus(x));

        break(x == 5) {
            total;

        }
        x = x + 1;
    }
    test_pointer_val + total;
}

fn get_weight(n: int32): int32 {
    1 + (n - ((n / 2) * 2) == 0);
}

fn print_char(c: char): int32 {
    asm "
        mov rax, 1          
        mov rdi, 1          
        lea rsi, [rbp-8]    
        mov rdx, 1          
        syscall
    ";
    0;
}
```

# Roadmap
## DONE:
* Variable support
* Functions
* Pointers
* Inline assembly support
* Loop/break/continue
* sizeof/alignof
* Function and variable shadowing

## In Progress
* Custom IR, Build Target system

## Planned
* Structs and packed structs
* import system
* Standard library (StdLib)
* Optimization Passes
* Self-hosting (Rewriting the compiler in Hupeyaszih Programming Language)
# The Philosophy
Hupeyaszih aims to empower developers to create branchless, high-performance software with zero implicit overhead. No hidden branches, no implicit overhead. What you see is what the CPU executes.


# Project structures
```
.
├── build/          # Where you build the compiler (cmake)
│   
├── example/        # .hrs example files
├── include/
│   ├── backend/
│   ├── core/
│   └── opt/
├── out/            # Default output directory for compiled assembly
└── src/            # Compiler source code (C)
    ├── backend/
    ├── core/
    └── opt/
```

# Installation
```bash
git clone https://github.com/hupeyaszih/hupeyaszih-programming-language.git
cd hupeyaszih-programming-language
mkdir build && cd build
cmake ..
make
```

# Usage
To compile a Hupeyaszih (.hrs) file:
```bash
./hrsc <file_path>.hrs -o <file_path>.asm --run --clean
```

Example: 
```bash
./hrsc ../example/testing.hrs -o ../out/testing.asm --run --clean
```

# Execute the Compiled code
(You can use "--run" flag to do this automaticly) Change directory to your output path and execute:
```bash
nasm -f elf64 <file_name>.asm -o <file_name>.o
ld <file_name>.o -o <file_name>
./<file_name>; echo $?
```


