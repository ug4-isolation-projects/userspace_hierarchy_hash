# Userspace hierarchical hash table

A testbench to have a look at the viability of a hierarchical hash table, to be used as a basis of a modification of the futex hash table in the linux kernel.

## Installation

Run the following:

`gcc main.c hash.c jhash.c -o main`

Alternatively, run `make`.

### Usage

The program allows the following commands:

+ add: With a value and a uid, insert an entry into the table.
+ remove: With a key and a uid, remove an entry from the table.
+ print: Print a sub bucket.
+ resize: Tests the function of the resizing (increase+decrease) in the hash table.
+ exit
