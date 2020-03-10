import os
import sys

import respire.buildlib.cc_toolchains.clang as clang
import respire.buildlib.cc_toolchains.gcc as gcc
import respire.buildlib.cc_toolchains.msvc as msvc

def DiscoverHostToolchain():
  '''Searches the system for a default host toolchain, and returns it.'''

  toolchain = None
  if os.name == 'posix':
    CLANG_DEFAULT_PATH = '/usr/bin/clang'
    GCC_DEFAULT_PATH = '/usr/bin/gcc'
    if os.path.exists(CLANG_DEFAULT_PATH):
      toolchain = clang.ClangToolchain(CLANG_DEFAULT_PATH)
    elif os.path.exists(GCC_DEFAULT_PATH):
      toolchain = gcc.GccToolchain(GCC_DEFAULT_PATH)
  elif sys.platform == 'win32':
    toolchain = msvc.DiscoverHostMSVC()

  if not toolchain:
    raise Exception('Failed to discover a toolchain in this environment.')

  return toolchain
