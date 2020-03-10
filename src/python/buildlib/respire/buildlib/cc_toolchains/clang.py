import os

import respire.buildlib.cc_toolchains.gcc as gcc

class ClangToolchain(gcc.GccToolchain):
  def __init__(self, clang_executable_path):
    clang_executable_path = clang_executable_path
    clang_splitext_path = os.path.splitext(clang_executable_path)
    clangpp_executable_path = (
        clang_splitext_path[0] + '++' + clang_splitext_path[1])
    clang_ar_executable_path = (
        os.path.join(os.path.dirname(clang_executable_path),
                     'ar' + clang_splitext_path[1]))
    super(ClangToolchain, self).__init__(
        clang_executable_path,
        gpp_executable_path=clangpp_executable_path,
        ar_executable_path=clang_ar_executable_path)
