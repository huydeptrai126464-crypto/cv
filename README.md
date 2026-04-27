# Cv 1.0

Cv stands for Compiler Virtual Machine.

Cv is a tiny language, a bytecode compiler, and a virtual machine designed to experiment with simple source code, compile it into readable bytecode, and run it through a VM to get the final result.

This project is still very young and weak, so expect limitations, rough edges, and room for experimentation.

---

## Overview

Cv uses this pipeline:

`file.cv -> bytecode -> program.bc -> VM -> result`

First, you write a source file such as `file.cv`.

Then the bytecode compiler reads that file and compiles it into an intermediate file named `program.bc`.

After that, the VM reads `program.bc` and executes it to display the result.

The important thing is that `program.bc` is not a binary file. It is a human-readable intermediate bytecode file, so you can open it and see exactly what the compiler produced.

---

## How the language works

Cv source code starts from the `__start__:` label.

This is similar to `main()` in C++.

Everything that should run first must be placed under `__start__:`.

Functions are declared with `@`.

The division operator is `#`.

Comments use `//`.

---

## File structure

A Cv source file can contain:

- a `__start__:` entry label
- labels ending with `:`
- function blocks using `@name { ... }`
- instructions such as `push`, `print`, `add`, `jmp`, `call`, and more
- comments using `//`

Example:

```cv
__start__:
push 5
push 3
add
print
halt
```

---

## Syntax

### Entry point

The program must contain:

```cv
__start__:
```

This is the entry label of the program.

Everything after `__start__:` belongs to the main execution flow.

---

### Labels

Labels end with a colon:

```cv
loop:
```

Labels are used as jump targets for:

- `jmp`
- `jz`
- `jnz`
- `call`

---

### Functions

Functions are written like this:

```cv
@hello {
    push 10
    print
}
```

A function name starts with `@`, followed by the name, then `{`, and ends with `}`.

Functions are compiled separately and can be called with:

```cv
call hello
```

---

### Comments

Cv comments use `//`.

Anything after `//` on the same line is ignored.

Example:

```cv
push 5 // this is a comment
```

---

## Expressions

Cv supports expressions inside `push` and `print`.

Supported operators:

- `+`
- `-`
- `*`
- `#` for division
- parentheses `(` and `)`

Example:

```cv
push 2 + 3 * (4 # 2)
print 10 + 5
```

The expression compiler follows normal precedence rules:

1. Parentheses
2. Unary minus
3. `*` and `#`
4. `+` and `-`

Unary minus is supported too:

```cv
push -5
```

---

## Instructions

### Stack operations

#### `push <expr>`
Pushes the result of an expression onto the stack.

Example:

```cv
push 10
push 2 + 3
```

#### `pop`
Removes the top value from the stack.

#### `dup`
Duplicates the top value of the stack.

#### `swap`
Swaps the top two values of the stack.

---

### Arithmetic operations

#### `add`
Pops the top two values and pushes the sum.

#### `sub`
Pops the top two values and pushes the difference.

#### `mul`
Pops the top two values and pushes the product.

#### `div`
Pops the top two values and pushes the quotient.

#### `gt`
Compares two values, pushes 1 if the first is greater, otherwise 0.

#### `lt`
Compares two values, pushes 1 if the first is less, otherwise 0.

#### `eq`
Compares two values, pushes 1 if they are equal, otherwise 0.

---

### Output

#### `print`
Prints the top value of the stack.

You can also give it an expression:

```cv
print 1 + 2 * 3
```

That expression is compiled first, then the result is printed.

---

### Flow control

#### `jmp <label>`
Unconditional jump to a label.

#### `jz <label>`
Jump if zero.

#### `jnz <label>`
Jump if not zero.

#### `call <label>`
Calls a function or labeled block.

#### `ret`
Returns from a function.

#### `halt`
Stops execution.

---

## Compiler flow

Cv follows this process:

1. Read the `.cv` source file
2. Compile it into bytecode
3. Save the result into `program.bc`
4. Run the VM on `program.bc`
5. Print the final result

So the compiler is not executing your code directly. It is translating your source into an intermediate bytecode file first.

That means you can inspect `program.bc` and see what the compiler actually produced.

---

## Usage

### Compile source into bytecode

```bash
./bytecode file.cv
```

This will create:

```bash
program.bc
```

If you do not provide a filename, the compiler uses the default source file:

```bash
code.cv
```

and outputs:

```bash
program.bc
```

---

### Run the VM

```bash
./vm program.bc
```

The VM reads the compiled bytecode file and prints the result.

---

### One-step version

If you do not want to run the compiler and VM separately, use the prebuilt `Cv.cpp` version:

```bash
./cv file.cv
```

This automatically compiles the source and runs it.

It will still create `program.bc`, so be careful and remember that the intermediate file is always generated.

---

## Important warning

Every step needs a parameter.

That means:

- the compiler needs a source file
- the VM needs a bytecode file
- the one-step runner needs a source file

Examples:

```bash
./bytecode file.cv
./vm program.bc
./cv file.cv
```

---

## Example program

```cv
__start__:
push 5
push 7
add
print
halt
```

This will output:

```text
12
```

---

## Example with a function

```cv
@show {
    push 100
    print
}

__start__:
call show
halt
```

---

## Example with a loop

```cv
__start__:
push 3

loop:
dup
print
push 1
sub
dup
jnz loop
pop
halt
```

---

## Why `program.bc` is useful

The generated `program.bc` file is readable, so you can inspect the compiler output without dealing with raw binary.

This helps you:

- debug the compiler
- understand how source code becomes bytecode
- study what the VM will execute
- improve the language step by step

---

## Current status

Cv is still weak and experimental.

That means:

- the language is small
- the compiler is simple
- the VM is still evolving
- new features can be added later

But this is also what makes it interesting: you can see the whole pipeline clearly, from source code to result.

---

## Summary

Cv 1.0 is a small compiler and VM system.

- `__start__:` works like `main()`
- functions use `@name { ... }`
- division uses `#`
- comments use `//`
- source files are compiled into readable `program.bc`
- the VM runs `program.bc`
- you can use `./bytecode file.cv`, then `./vm program.bc`
- or use `./cv file.cv` for the full automatic flow

Cv is simple, experimental, and built for learning and exploring bytecode.

