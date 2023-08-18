# Pelago

Pelago is the toolchain used by the [Data Intensive Applications & Systems Lab (DIAS) lab](https://www.epfl.ch/labs/dias/) at EPFL for C++ projects, primarily [Proteus](https://github.com/epfl-dias/proteus).

The toolchain is used for dependencies required before CMake configuration time, such as CMake itself and compilers. Dependencies are pulled in as git submodules under `external/` or downloaded directly through Make.


## Requirements
* Ubuntu 18.04 LTS
* GNU Make
* Git >= 2.17 (to checkout this project and dependencies)
* curl

For LLVM:
* C++ compiler & tools to bootstrap LLVM
* libxml2-dev

## Building the toolchain

To build the project, call `make`. Everything will be downloaded, configured, built and installed into `pelago/opt` automatically.