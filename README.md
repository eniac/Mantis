## P4R frontend

### Prerequisites

Flex, Bison, g++ (version >=4.9)

### Contents

- `examples/`: Sample p4r programs
- `src/`: Source code for P4R frontend
	- `ast/`: AST nodes for P4 and P4R
	- `compile/`: P4/C compilation passes
- `include/`: Header files included by bison parser
- `frontend.l`: flex tokenizer
- `frontend.y`: bison grammar parser

### Usage 

P4R frontend is a lightweight Flex-bison based compiler that converts a P4R program into two files:

1. a tofino-compatible P4-14 program targeting Tofino P4-14: `<p4r_prog_name>_mantis.p4`
2. a prologue/dialogue implementation in C: `p4r.c`

Arguments to the frontend are: 

- ```-i```: file path for input p4r
- ```-o```: file base name for C/P4 output

The compiler first transforms P4R code into a syntax tree, then *interprets* the syntax tree with multiple passes that add new code and transform existing code.
Finally, it dumps the P4, C from the syntax tree to separate output files.
The frontend generated C file is later forwarded to C preprocessor to render the actual implementation.
Later Mantis will generate and load the shared object and link it to the run time agent.

`compile_p4r.sh` wraps the usage of frontend.
For example, to compile `examples/table_add_del_mod.p4r` to the tofino sde tree:

```
sudo ./compile_p4r.sh examples/table_add_del_mod.p4r ./table_add_del_mod
```