# Respire 

![C/C++ CI](https://github.com/aabtop/respire/workflows/C/C++%20CI/badge.svg)

## Introduction

Respire provides logic to describe and execute an arbitrary file dependency
graph.  It is a build system tool, like
[make](https://www.gnu.org/software/make/), however unlike make it offers the
full power of [Python](https://www.python.org/) to setup build rules enabling a
more high level description of a dependency graph.  Respire is compatible with
both Python 2 and 3.

[SCons](https://scons.org/) is a similar build system tool that offers the
expressiveness of Python to describe a build dependency graph, however SCons
is known to be slow to execute.  On the other hand, Respire caches the results
of executed Python scripts into easy to parse low-level file dependencies so
that unless the Python logic is modified, the Python interpreter will not need
to be invoked.  This low-level cached file format (currently stored as JSON)
serves a similar role as the input to the [ninja](https://ninja-build.org/)
build system, in that it's meant to be parsed quickly, and not to be analyzed by
humans.

A core set of functionality is provided enabling direct specification of a
per-file dependency graph.  It can be tedious to define large projects in this
way, so higher-level utilities are available on top of the core functionality.
This enables, for example, C++ projects to be defined in terms of modules, like
executables, static libraries and dynamic libraries.  No magic is involved in
the creation of these libraries, and so anyone is free to build their own in
order to create a domain specific language for specifying build targets for
custom tools or other programming languages.

## Building Respire

While Respire can be built by Respire, it can also be built using
[CMake](https://cmake.org/), in order to bootstrap the process.

### Build with CMake

There is a [CMakeLists.txt](src/CMakeLists.txt) file in the [src/](src)
directory.  Respire is confirmed to be buildable on Windows using
[MSVC](https://visualstudio.microsoft.com/vs/features/cplusplus/) and on
Linux with [g++](https://gcc.gnu.org/).

Once built, you will need to install the result, e.g. by running

```
cmake --build . --target install
```

and then add the `${CMAKE_OUT}/package/python` directory to your `PYTHONPATH`
environment variable.

Typically, Respire projects provide a build entry point Python script which
must be executed in order to kick off the build.  See
[src/example/SimpleMultiSourceCPP/build.py](src/example/SimpleMultiSourceCPP/build.py)
for an example entry point script.

### Build with Respire

Once Respire is installed on a system (e.g. see
[Build with CMake](#Build-With-CMake)), then Respire can be built using itself
by running:

```
python src/build.py
```

By default, the output will appear in a newly created `out/` folder. By default
a debug build is generated and the Respire binary executable is not packaged
with the associated Python files.  In order to produce the final packaged
output, call:

```
python src/build.py -t package
```

which will output the results into the directory `out/debug/package` (e.g. you
could set your `PYTHONPATH` to this directory in order to use this new instance
of Respire for future Respire builds).

In order to build a release build of respire, and package the results, call

```
python src/build.py -t package -c release
```

and the results will be available in `out/release/package`.

## Example

### Small C++ Project

The directory
[src/example/SimpleMultiSourceCPP](src/example/SimpleMultiSourceCPP) contains
a small example of how to use Respire's high level C++ layer to define a
small C++ project.
