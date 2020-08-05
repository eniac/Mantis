## Mantis

### Description

P4R frontend is a lightweight flex-bison based compiler that converts a P4R program into two files:

1. a tofino-compatible P4-14 program targeting Wedge100BF-32X Tofino switch: `<p4r_prog_name>_mantis.p4`
2. a prologue/dialogue implementation in C: `p4r.c`

The compiler first transforms P4R code into a syntax tree, then *interprets* the syntax tree with multiple passes that adds new code and transforms the existing code.
Finally, it dumps the P4, C from the syntax tree to separate output files.
The frontend generated C file is later forwarded to C preprocessor to render the actual implementation.
Later Mantis will generate and load the shared object and link it to the run time agent.

### Prerequisites

* Flex, Bison, g++ (version >=4.9)
* Barefoot SDE (optional, if further compile the generated malleable P4 code to tofino ASIC or its simulation model or launch an agent on switch onboard CPU, the full implemention is tested on 9.0.0)

### Contents

- `examples/`: Sample p4r programs
- `src/`: Source code for P4R frontend
	- `ast/`: AST nodes for P4 and P4R
	- `compile/`: P4/C compilation passes
- `include/`: Header files included by bison parser
- `frontend.l`: flex tokenizer
- `frontend.y`: bison grammar parser
- `agent/`: Mantis agent related

### How to Run

Point `SDE` variable to the SDE root directory, e.g.,

```
export SDE="/home/mantis/bf-sde-9.0.0"
```

#### Frontend

`compile_p4r.sh` wraps the usage of frontend: `./compile_p4r.sh [-vv] [-o output_dir] input_file`

Arguments to the script are: 

- ```input_file```: file path for input p4r
- ```-o```: optional flag to specify the target output directory, by default, the generated files will be at `out/` directory
- ```-vv```: optional flag to view info/verbose output

For example, to compile `examples/dos.p4r` to the default `out/` directory with verbose flag:
```
sudo -E ./compile_p4r.sh -vv examples/dos.p4r
```

`compile_p4r.sh` also links the compilation of the malleable P4 code at the end, one could comment out the last section when a tofino switch/similator is not available.

#### Agent

* `launch.sh` wraps the launch of a Mantis controller instance: `sudo -E ./launch.sh <p4 prog name>`
* `run.sh` wraps the generation of the shared object and the activation of reaction loops: `sudo -E ./run.sh <p4 prog name> <p4r.c path>`

E.g., for `dos.p4r` example

```
# Set SDE variable for each session
# Terminal 1
sudo -E ./launch.sh dos_mantis
# Terminal 2, AFTER the completion of ./launch.sh warming up, around 10s
sudo -E ./run.sh dos_mantis ../out/p4r.c
```

### Further Questions

A practical [tutorial](https://github.com/eniac/Mantis/blob/master/tutorial.md) is attached, for more details, please refer to the paper [Mantis: Reactive Programmable Switches](https://dl.acm.org/doi/10.1145/3387514.3405870) (SIGCOMM '20).

Feel free to post [issues](https://github.com/eniac/Mantis/issues) if any question arises.
