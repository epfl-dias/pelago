# pelaGos Project

## Requirements

 * Ubuntu 16.04 LTS
 * CMake 3
 * GNU Make
 * git (to checkout this project and dependencies)
 * C++ compiler & tools to bootstrap LLVM

## Building the project

To build the project, call `make`. Everything will be downloaded,
configured, built and installed automatically.

To reduce development cycles each steps is marked as done by creating an
empty file in the root directory. To retrigger a step, and all the
following steps a simple:

```sh
    $ touch <project>.<step>_done
```

The build will be exectude using several default values, which can be
changed by specifying them in one of the following ways:

 * $ export VAR=value
   $ make
 * $ VAR=value make
 * $ make VAR=value

To see the variables and their current values:
```sh
    $ make showvars
```

## Folder layout

### After checkout of the project

```
    .
    ├── Makefile
    ├── README.md
    └── src
        ├── postgres
        └── raw-jit-executor
    
    3 directories, 2 files
```

### After a successfull build of the project

A set of `<project>.<step>_done` files will be generated at the root.

Using the default settings, the following folder hierarchy will be created:
```
    .
    ├── build
    │   ├── glog
    │   ├── gtest
    │   ├── llvm
    │   ├── rapidjson
    │   └── raw-jit-executor
    ├── opt
    │   ├── bin
    │   ├── include
    │   ├── lib
    │   ├── libexec
    │   ├── raw
    │   └── share
    └── src
        ├── clang
        ├── compiler-rt
        ├── coordinator
        ├── glog
        ├── gtest
        ├── libcxx
        ├── libcxxabi
        ├── libunwind
        ├── llvm
        ├── rapidjson
        └── raw-jit-executor

    25 directories
```
