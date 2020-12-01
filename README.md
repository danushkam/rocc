## Introduction

This code repository includes the OMNeT++ implementations of RoCC and the main reference solutions -- DCQCN and HPCC -- we use in the paper. This documentation explains how to compile and run the simulations.

## Compile

Please follow the step given below to install the simulation environment.

1. Install OMNeT++ version 5.2.1. Please download it from https://omnetpp.org/download/old and follow the simple instructions given in the documentation to install it on your preferred operating system.

2. Once you have OMNeT++ working on your machine, you can compile the code in this repo in the order given below. You can use the make files in the respective directories to compile the binaries by running *make*. Alternatively, you can import the modules into the OMNeT++ IDE (*File -> Open Projects from File System...*) and compile them using the IDE.

- inet-3.6.4
- commons
- dcqcn
- hpcc
- rocc

## Run Simulations

You can use the *.ini* files inside the solution directories to run the simulations using the OMNeT++ IDE. The repository currently includes the simulations for evaluating the solution implementations and we are in the process of adding the rest of the simulations into the repo.

Please feel free to contact me at *dmenikku@purdue.edu* if you have any questions on compiling or running the simulations.
