# pelagos Project

## Requirements

 * Ubuntu 16.04 LTS
 * CMake 3
 * GNU Make
 * git (to checkout this project and dependencies)
 * C++ compiler & tools to bootstrap LLVM
 * And any other packages listed in `requirements.txt`
    * Under Ubuntu you can install them using: `apt install $(cat requirements.txt)`

## Online help

You can access the online help with:

```sh
   $ make help
```

## Building the project

To build the project, call `make`. Everything will be downloaded, configured, built and installed automatically.

In order to speed up the development cycles each steps is marked as done by creating a hidden empty file in the root directory. To retrigger a step, and all the following ones use a simple:

```sh
    $ touch .<project>.<step>_done
```

To rebuild a specific external target, you can use:

```sh
    $ make clean-$(target)
```

The build will be exectude using several default values, which can be
changed by specifying them in one of the following ways:

 * $ export VAR=value
   $ make
 * $ VAR=value make
 * $ make VAR=value

To see the variables and their current values:
```sh
    $ make show-config
```

### Install coordinator dependencies

```sh
    pip install --user --upgrade < src/coordinator/requirements.txt
```

## Folder layout

### After checkout of the project

```
    .
    ├── Makefile
    ├── README.md
    └── src
        └── coordinator
```

### After a successfull build of the project

Using the default settings, the following folder hierarchy will be created:
```shell
    .
    ├── build
    │   ├── glog
    │   ├── gtest
    │   ├── llvm
    │   ├── rapidjson
    │   └── raw-jit-executor
    ├── external
    │   ├── bsd
    │   │   ├── clang
    │   │   ├── compiler-rt
    │   │   ├── glog
    │   │   ├── gtest
    │   │   ├── libcxx
    │   │   ├── libcxxabi
    │   │   ├── libunwind
    │   │   └── llvm
    │   └── mit
    │       └── rapidjson
    ├── opt
    │   ├── bin
    │   ├── include
    │   ├── lib
    │   ├── libexec
    │   ├── raw
    │   └── share
    └── src
        ├── coordinator
        ├── panorama
        ├── planner
        └── raw-jit-executor
```
