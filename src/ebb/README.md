# Ebb

## Introduction

Ebb is a C library allowing you to build streaming data flow and computation
dependency graphs.  It can for example be leveraged to create a build system.
It is built on top of coroutines to allow for many user-space light-weight
tasks to be created simultaneously.

Ebb has been carefully designed to minimize the number of heap allocations
that are required by the library.  In many cases, state can be stored in
a fiber's stack, and tracked objects maintain linked list "next pointers"
in order to avoid the need for an explicit container.

## Modifying the code

Ebb uses the [git subrepo](https://github.com/ingydotnet/git-subrepo) extension
to manage its third party dependencies.  You do not need to download this
unless you are intending to manage these third party repositories within
the Ebb repository.
