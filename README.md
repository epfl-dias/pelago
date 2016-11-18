# pelaGos Project

This project uses the PostgreSQL frontend (SQL parser, and query
optimizer) and connects it to the ViDa engine, for an end-to-end
experience.

## Requirements

 * GNU Make
 * git (to checkout this project and dependencies)
 * svn (to checkout & build LLVM)
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
    │   ├── postgres
    │   ├── rapidjson
    │   └── raw-jit-executor
    ├── opt
    │   ├── bin
    │   ├── include
    │   ├── lib
    │   ├── raw-jit-executor -> /localhome/sambuc/Projects/pelaGo/build/raw-jit-executor
    │   └── share
    └── src
        ├── glog
        ├── gtest
        ├── llvm
        ├── postgres
        ├── rapidjson
        └── raw-jit-executor
    
    20 directories
```
