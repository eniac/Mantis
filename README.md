## P4R frontend

### Prerequisites

* Flex, Bison, g++ (version >=4.9)
* Barefoot SDE 9.0.0 (optional, to further compile the generated malleable P4 code to tofino)

### Contents

- `examples/`: Sample p4r programs
- `src/`: Source code for P4R frontend
	- `ast/`: AST nodes for P4 and P4R
	- `compile/`: P4/C compilation passes
- `include/`: Header files included by bison parser
- `frontend.l`: flex tokenizer
- `frontend.y`: bison grammar parser

### Description

P4R frontend is a lightweight Flex-bison based compiler that converts a P4R program into two files:

1. a tofino-compatible P4-14 program targeting Tofino P4-14: `<p4r_prog_name>_mantis.p4`
2. a prologue/dialogue implementation in C: `p4r.c`

The compiler first transforms P4R code into a syntax tree, then *interprets* the syntax tree with multiple passes that adds new code and transforms the existing code.
Finally, it dumps the P4, C from the syntax tree to separate output files.
The frontend generated C file is later forwarded to C preprocessor to render the actual implementation.
Later Mantis will generate and load the shared object and link it to the run time agent.

### How to run

`compile_p4r.sh` wraps the usage of frontend `./compile_p4r.sh [-vv] [-o output_dir] input_file`.

Arguments to the script are: 

- ```input_file```: file path for input p4r
- ```-o```: optional flag to specify the target output directory, by default, the generated files will be at `out/` directory
- ```-vv```: optional flag to view info/verbose output

For example, to compile `examples/failover_tstamp.p4r` to the default `out/` directory with verbose flag:
```
sudo ./compile_p4r.sh -vv examples/failover_tstamp.p4r
```
