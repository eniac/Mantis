# Tutorial

P4R is a simple extension to P4 language to express fine-grained, serializable, and switch-local reactive behaviors.

## Writing a P4R Program From Scratch

P4R program can be decomposed into 3 steps.

#### S1: Static P4 Code

First, one writes a P4 code (specifically P4-14) as usual which defines a static packet processing behavior.
Note that currently the compiler targets at P4-14 on Wedge100BF-32X Tofino switch.

#### S2: Specifying Malleable Entities

One could define malleable entities that are amenable to run time reconfiguration, for example:

* [figure1.p4r](https://github.com/eniac/Mantis/blob/master/examples/figure4.p4r) shows a simple example declaring a malleable value `value_var` and uses it in `my_action` to change the variable added to `hdr.foo`.
* [figure5.p4r](https://github.com/eniac/Mantis/blob/master/examples/figure5.p4r) defines a malleable field `write_var` and uses it in `my_action` so that `baz` (right hand side) is assigned to one of its `alts`.
* [figure6.p4r](https://github.com/eniac/Mantis/blob/master/examples/figure6.p4r) also defines a malleble field `read_var` but uses it at the left hand side in an addition and `my_table` match, one could later change the references in the reaction during run time. 
* [mbl\_table.p4r](https://github.com/eniac/Mantis/blob/master/examples/mbl_table.p4r) defines a malleable table `ti_var_table` that is amenable to fine-grained manipulations ensuring serializability.

#### S3: Define Reaction

Reaction function is a C-like function, there are 3 components to specify: reaction arguments (objects in the data plane), control logic, and reconfiguration.

*Reaction Arguments*

Reaction arguments are used to specify the data plane metrics to be accessed as C variables/arrays, Mantis will take caure of the packing and the serializable reads underneath.
For example:

* [field\_arg.p4r](https://github.com/eniac/Mantis/blob/master/examples/field_arg.p4r) specifies field arguments of `hdr.foo` etc and accesses their values as normal C variables.
* [failover\_tstamp.p4r](https://github.com/eniac/Mantis/blob/master/examples/failover_tstamp.p4r) specifies register arguments `ri_pkt_counter`, `ri_ingress_tstamp` and reads them as C arrays in the function.

*Control Logic*

One could specify arbitrary C code in the reaction function. Besides, one could specify static variables to retain stateful values across dialogues.

*Reconfiguration*

* To reconfigure malleable values, fields, one could leverage a simple syntax `<mbl>=<value>`, e.g., [figure1.p4r](https://github.com/eniac/Mantis/blob/master/examples/figure4.p4r).
* To reconfigure tables, one could use a simple syntax: `<table name>_<add or mod or del>_<action name>(<operation index>, [match arguments], [priority, if ternary], [action arguments])`. [table\_add\_del\_mod.p4r](https://github.com/eniac/Mantis/blob/master/examples/table_add_del_mod.p4r) and [mbl\_table.p4r](https://github.com/eniac/Mantis/blob/master/examples/mbl_table.p4r) are examples showing the usage.

Note that one could also specify a `init_block` besides `reaction`, which could be handy for setting up initial table entries, as in example [table\_add\_del\_mod.p4r](https://github.com/eniac/Mantis/blob/master/examples/table_add_del_mod.p4r) and [mbl\_table.p4r](https://github.com/eniac/Mantis/blob/master/examples/mbl_table.p4r).

## A Practical Example

[dos.p4r](https://github.com/eniac/Mantis/blob/master/examples/dos.p4r) implements a simple Denial-of-Service reactive behavior that detects/blocks malicious flows with a simple threshold based approach.

#### Example Setups

*Topology*

```
S0(10.1.1.3)---switch 25Gbps (malicious sender)
S1(10.1.1.5)---switch 10Gbps (benign sender)
switch---D0(10.1.1.2) 10Gbps
```

*Configuration*

Based on the testbed environment, set up the ports as in [pi\_setup.py](https://github.com/eniac/Mantis/blob/master/agent/pi_setup.py) and entries for connectivity (via RPC calls in [pi\_setup.py](https://github.com/eniac/Mantis/blob/master/agent/pi_setup.py), interactive switch commands, or `init_block` in p4r).

Then run Mantis as in [How to Run](https://github.com/eniac/Mantis#how-to-run).

We configure [DPDK](https://github.com/jsonch/dpdkScripts) senders for S0 and S1 with scripts [benign.dpdk](https://github.com/eniac/Mantis/blob/master/util/benign.dpdk) and [malicious.dpdk](https://github.com/eniac/Mantis/blob/master/util/malicious.dpdk).

Note that this simple example doesn't add ARP protocol support, one needs to configure the static mappings for the servers.

#### Expectation

* The Mantis agent terminal should prints the infomation on new flows and the malicious flow (which is blocked).
* At D0, one could record the pcap trace before launching senders via `sudo tshark -s 64 -B 1024 -i enp101s0f1 > tmp` and verify that after a small time window, packets from S0 no longer reach D0.
