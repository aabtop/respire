import os
import shutil

import respire.buildlib.cc_toolchains.gcc as gcc

_DEFAULT_RASPI_TOOLS_ENVIRONMENT_VARIABLE = 'RASPI_TOOLS'
_DEFAULT_RASPI_SYSROOT_ENVIRONMENT_VARIABLE = 'RASPI_SYSROOT'

class RaspberryPiToolchain(gcc.GccToolchain):
  def __init__(self, raspi_tools_path=None, raspi_sysroot_path=None):
    if not raspi_tools_path:
      if _DEFAULT_RASPI_TOOLS_ENVIRONMENT_VARIABLE not in os.environ:
        print('Set the ' + _DEFAULT_RASPI_TOOLS_ENVIRONMENT_VARIABLE +
              ' environment variable to the location that' +
              ' you cloned https://github.com/raspberrypi/tools in to.')
        assert False
      raspi_tools_path = os.environ[_DEFAULT_RASPI_TOOLS_ENVIRONMENT_VARIABLE]

    if not raspi_sysroot_path:
      if _DEFAULT_RASPI_SYSROOT_ENVIRONMENT_VARIABLE not in os.environ:
        print('Set the ' + _DEFAULT_RASPI_SYSROOT_ENVIRONMENT_VARIABLE +
              ' environment variable to the location that you' +
              ' installed the raspi sysroot too (e.g. rsync from a device).')
        assert False
      raspi_sysroot_path = (
          os.environ[_DEFAULT_RASPI_SYSROOT_ENVIRONMENT_VARIABLE])

    compiler_path = os.path.join(
        raspi_tools_path, 'arm-bcm2708', 'arm-rpi-4.9.3-linux-gnueabihf',
        'bin', 'arm-linux-gnueabihf-gcc')

    usr_lib_path = os.path.join(
        raspi_sysroot_path, 'usr', 'lib', 'arm-linux-gnueabihf')
    extra_library_paths = [
        usr_lib_path,
        os.path.join(
            raspi_sysroot_path, 'lib', 'arm-linux-gnueabihf'),
    ]

    super(RaspberryPiToolchain, self).__init__(compiler_path)

    self.additional_compiler_flags = [
      '--sysroot=' + raspi_sysroot_path]
    self.additional_linker_flags = ([
      '--sysroot=' + raspi_sysroot_path] +
      ['-L' + x for x in extra_library_paths])

    # For some reason the raspberry pi cross-compiler requires these object
    # files to be available, but won't search for it in the path where it's
    # provided, so we copy it to a search path in the sysroot directory.  This
    # seems to only be necessary for building shared libraries.
    system_o_files = ['crti.o', 'crtn.o', 'crt1.o']
    system_o_dest = os.path.join(raspi_sysroot_path, 'usr', 'lib')
    system_o_src = usr_lib_path
    for o_file in system_o_files:
      dest = os.path.join(system_o_dest, o_file)
      src = os.path.join(system_o_src, o_file)
      if not os.path.exists(dest):
        shutil.copyfile(src, dest)

    self.raspi_sysroot_path = raspi_sysroot_path
    self.raspi_tools_path = raspi_tools_path

    self.toolchain_include_directories = (
        [os.path.join(self.raspi_sysroot_path, x) for x in [
          'opt/vc/include',
          'opt/vc/include/interface/vcos/pthreads',
          'opt/vc/include/interface/vmcs_host/linux',
          'usr/include/arm-linux-gnueabihf']])
    self.toolchain_library_directories = ([
        os.path.join(self.raspi_sysroot_path, 'opt/vc/lib')] +
        extra_library_paths)
