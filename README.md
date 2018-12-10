# pelagos Project

## Requirements

 * Ubuntu 16.04 LTS
 * GNU Make
 * Git (to checkout this project and dependencies)
 * sqlline (for the interactive client)

For raw-jit-executor:
 * libnuma1
 * libnuma-dev

For Panorama, sqlplanner & planner:
 * scala & sbt
 * npm

For Google Tests:
 * autoconf
 * libtool

For LLVM:
 * CMake 3
 * C++ compiler & tools to bootstrap LLVM
 * libxml2

## Online help

You can access the online help with:

```sh
   $ make help
```

## Ubuntu instruction to install scala and SBT

For Scala:

```sh
    $ sudo apt install scala scala-doc
```

For SBT:

```sh
    $ echo "deb https://dl.bintray.com/sbt/debian /" | sudo tee -a /etc/apt/sources.list.d/sbt.list
    $ sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv 2EE0EA64E40A89B84B2DF73499E82A75642AC823
    $ sudo apt-get update
    $ sudo apt-get install sbt
```

## Building the project

To build the project, call `make`. Everything will be downloaded, configured,
built and installed automatically.

You can start the system with 2 terminals: 
 1. `make run-server` in the first terminal 
 2. `make run-client` in the second terminal

In order to speed up the development cycles each steps is marked as done by
creating a hidden empty file in the root directory: `.<project>.<step>_done`.

You can retrigger any step among (checkout, configure, build, install) manually.
Please refer to `make help` for more details.

The build will be executed using several default values, which can be
changed by specifying them in one of the following ways:

 * `export VAR=value; make`
 * `VAR=value make`
 * `make VAR=value`

To see the variables and their current values:
```sh
    $ make show-config
```

### Install coordinator dependencies

```sh
    pip install --user --upgrade --requirement src/coordinator/requirements.txt
```

## Folder layout

### After checkout of the project

```
    .
    ├── Makefile
    ├── README.md
    ├── patches
    └── src
        ├── coordinator
        │   └── translator_scripts
        └── docker
            └── pelago-build
```

### After a successfull build of the project

Using the default settings, here is a summary of the folder hierarchy which
will be created:
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
    │   │   ├── clang-tools-extra
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
    ├── patches
    └── src
        ├── coordinator
        │   └── translator_scripts
        ├── docker
        │   └── pelago-build
        ├── panorama
        ├── planner
        ├── raw-jit-executor
        └── SQLPlanner
```
