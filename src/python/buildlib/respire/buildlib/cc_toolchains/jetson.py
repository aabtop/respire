import os

import respire.buildlib.cc_toolchains.gcc as gcc


_DEFAULT_JETSON_SYSROOT_ENVIRONMENT_VARIABLE = 'JETSON_SYSROOT'


class JetsonToolchain(gcc.GccToolchain):
  def __init__(self, toolchain_path=None, sysroot_path=None):
    if not toolchain_path:
      toolchain_path = os.path.join(
          os.sep, 'usr', 'bin', 'aarch64-linux-gnu-gcc')

    if not sysroot_path:
      if _DEFAULT_JETSON_SYSROOT_ENVIRONMENT_VARIABLE not in os.environ:
        print('Set the ' + _DEFAULT_JETSON_SYSROOT_ENVIRONMENT_VARIABLE +
              ' environment variable to the location that you' +
              ' installed the Jetson sysroot too (e.g. rsync from a device).')
        assert False
      sysroot_path = (
          os.environ[_DEFAULT_JETSON_SYSROOT_ENVIRONMENT_VARIABLE])

    super(JetsonToolchain, self).__init__(toolchain_path)

    self.additional_compiler_flags = [
      '--sysroot=' + sysroot_path]
    self.additional_linker_flags = [
      '--sysroot=' + sysroot_path]
    self.toolchain_library_directories = [
      sysroot_path,
      os.path.join(sysroot_path, 'usr', 'lib', 'aarch64-linux-gnu'),
      os.path.join(sysroot_path, 'lib', 'aarch64-linux-gnu')]
