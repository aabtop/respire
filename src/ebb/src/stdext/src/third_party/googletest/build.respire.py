from collections import namedtuple
import os

import respire.buildlib.modules as modules

def Build(registry, out_dir, configured_toolchain):
  if not os.path.exists(out_dir):
    os.makedirs(out_dir)

  gtest_configured_toolchain = configured_toolchain

  gtest_configured_toolchain.configuration.defines += [
      'GTEST_LANG_CXX11=1',
      'GTEST_API_=',
  ]

  gtest_configured_toolchain.configuration.include_directories = [
    os.path.join(os.path.dirname(__file__), 'googletest'),
  ]

  gtest_lib = modules.StaticLibraryModule(
      'gtest', registry, out_dir, gtest_configured_toolchain,
      sources=[
        'googletest/include/gtest/gtest-death-test.h',
        'googletest/include/gtest/gtest-message.h',
        'googletest/include/gtest/gtest-param-test.h',
        'googletest/include/gtest/gtest-printers.h',
        'googletest/include/gtest/gtest-spi.h',
        'googletest/include/gtest/gtest-test-part.h',
        'googletest/include/gtest/gtest-typed-test.h',
        'googletest/include/gtest/gtest.h',
        'googletest/include/gtest/gtest_pred_impl.h',
        'googletest/include/gtest/internal/gtest-death-test-internal.h',
        'googletest/include/gtest/internal/gtest-filepath.h',
        'googletest/include/gtest/internal/gtest-internal.h',
        'googletest/include/gtest/internal/gtest-linked_ptr.h',
        'googletest/include/gtest/internal/gtest-param-util-generated.h',
        'googletest/include/gtest/internal/gtest-param-util.h',
        'googletest/include/gtest/internal/gtest-port.h',
        'googletest/include/gtest/internal/gtest-string.h',
        'googletest/include/gtest/internal/gtest-tuple.h',
        'googletest/include/gtest/internal/gtest-type-util.h',
        'googletest/src/gtest-death-test.cc',
        'googletest/src/gtest-filepath.cc',
        'googletest/src/gtest-internal-inl.h',
        'googletest/src/gtest-port.cc',
        'googletest/src/gtest-printers.cc',
        'googletest/src/gtest-test-part.cc',
        'googletest/src/gtest-typed-test.cc',
        'googletest/src/gtest.cc',
      ],
      public_include_paths=[
        'googletest/include',
      ])

  gtest_main = modules.StaticLibraryModule(
      'gtest_main', registry, out_dir, gtest_configured_toolchain,
      sources=[
        'googletest/src/gtest_main.cc',
      ],
      module_dependencies=[
        gtest_lib,
      ])

  return {'gtest_main': gtest_main}
