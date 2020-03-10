import os
import sys

import respire.buildlib.cc as cc

def DiscoverHostMSVC():
  # First try to discover and use MSVC 15.
  msvc_15_toolchain = DiscoverMSVC15()
  if msvc_15_toolchain:
    return msvc_15_toolchain

  # If that doesn't work, fall back to MSVC 14.
  msvc_14_toolchain = DiscoverMSVC14()
  if msvc_14_toolchain:
    return msvc_14_toolchain

  # We failed to find a toolchain, return failure.
  return None


def DiscoverMSVC15():
  vc_dir = 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Tools\\MSVC\\14.12.25827'
  windows_kit_path = 'C:\\Program Files (x86)\\Windows Kits\\10'
  windows_kit_version = '10.0.16299.0'

  system_include_directories = [
    os.path.join(vc_dir, 'include'),
    os.path.join(windows_kit_path, 'Include', windows_kit_version, 'um'),
    os.path.join(windows_kit_path, 'Include', windows_kit_version, 'shared'),
    os.path.join(windows_kit_path, 'Include', windows_kit_version, 'ucrt'),
  ]

  system_library_directories = [
    os.path.join(vc_dir, 'lib', 'x86'),
    os.path.join(windows_kit_path, 'Lib', windows_kit_version, 'um', 'x86'),
    os.path.join(windows_kit_path, 'Lib', windows_kit_version, 'ucrt', 'x86'),
  ]

  try:
    return MSVCToolchain(
        os.path.join(vc_dir, 'bin', 'Hostx86', 'x86', 'cl.exe'),
        system_include_directories=system_include_directories,
        system_library_directories=system_library_directories)
  except:
    return None


def DiscoverMSVC14():
  vc_dir = 'C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC'
  windows_sdk_path = 'C:\\Program Files (x86)\\Windows Kits\\8.1'
  # Since Visual Studio 14.0, the CRT is no longer packaged with MSVC, it is
  # now packaged seperately, known as the Universal CRT SDK, and lives by
  # default in "C:\Program Files (x86)\Windows Kits\10".  See
  # https://blogs.msdn.microsoft.com/vcblog/2015/03/03/introducing-the-universal-crt/
  # for more details.
  # If these are left set to None, then they will not be included in the
  # include/library paths.
  universal_crt_windows_kit_path = 'C:\\Program Files (x86)\\Windows Kits\\10'
  universal_crt_version = '10.0.10240.0'

  system_include_directories = [
    os.path.join(vc_dir, 'include'),
    os.path.join(windows_sdk_path, 'Include', 'um'),
    os.path.join(windows_sdk_path, 'Include', 'shared'),
    os.path.join(universal_crt_windows_kit_path, 'Include',
                 universal_crt_version, 'ucrt'),
  ]

  system_library_directories = [
    os.path.join(vc_dir, 'lib'),
    os.path.join(windows_sdk_path, 'Lib', 'winv6.3', 'um', 'x86'),
    os.path.join(universal_crt_windows_kit_path, 'Lib', universal_crt_version,
                 'ucrt', 'x86')]

  try:
    return MSVCToolchain(
        os.path.join(vc_dir, 'bin', 'cl.exe'),
        system_include_directories=system_include_directories,
        system_library_directories=system_library_directories)
  except:
    return None


_WARNING_TO_WARNING_NUMBER_MAP = {
  cc.Configuration.WARNING_SIGNED_UNSIGNED_MISMATCH: ['4018'],
  cc.Configuration.WARNING_POSSIBLE_LOSS_OF_DATA_WHEN_CONVERTING:
      ['4244', '4305', '4267'],
  cc.Configuration.WARNING_NO_DEFINITION_FOR_INLINE_FUNCTION : ['4506'],
  cc.Configuration.WARNING_UNARY_MINUS_APPLIED_TO_UNSIGNED_TYPE : ['4146'],
  cc.Configuration.WARNING_SWITCH_STATEMENT_CONTAINS_ONLY_DEFAULT : ['4065'],
  cc.Configuration.WARNING_FORCING_VALUE_TO_BOOL : ['4800'],
}

class MSVCToolchain(cc.Toolchain):
  def __init__(self, cl_executable_path, system_include_directories=[],
               system_library_directories=[], link_executable_path=None,
               lib_executable_path=None, deps_wrapper_script=None):
    self.cl_executable_path = cl_executable_path
    self.deps_wrapper_script = deps_wrapper_script

    if not os.path.isfile(self.cl_executable_path):
      raise Exception('Path to "cl.exe" does not point to a file.')

    if link_executable_path:
      self.link_executable_path = link_executable_path
    else:
      self.link_executable_path = (
          os.path.join(os.path.dirname(self.cl_executable_path), 'link.exe'))

    if not os.path.isfile(self.link_executable_path):
      raise Exception('Could not find "link.exe".  Expected to find it next '
                      'to "cl.exe".')

    if lib_executable_path:
      self.lib_executable_path = lib_executable_path
    else:
      self.lib_executable_path = (
          os.path.join(os.path.dirname(self.cl_executable_path), 'lib.exe'))

    if not os.path.isfile(self.lib_executable_path):
      raise Exception('Could not find "lib.exe".  Expected to find it next '
                      'to "cl.exe".')

    self.system_include_directories = system_include_directories
    self.system_library_directories = system_library_directories

    # We need to wrap compilations with a script so that we can parse the
    # output of /showIncludes, and forward the remaining output onwards.
    self.deps_wrapper_script = os.path.join(os.path.dirname(__file__),
                                            'wrap_cl_for_deps.py')


  def Compile(self, registry, configuration, source_filepath, object_basepath,
              extra_dependencies=[]):
    object_filepath = object_basepath + '.obj'

    command = [
      self.cl_executable_path,
      # Suppress the MSVC header/version information from being printed.
      # It's just noise.
      '/nologo',
      # Indicate that we intend to compile a single file into a obj file without
      # performing any linking.
      '/c',
      # Explicitly indicate that all source files are C++ source files.
      '/TP',
      # Disable code analysis warnings by default to make default output flow
      # more similar to other compilers.
      '/analyze-',
      # Choose warning level 3, which has been empirically found to be similar
      # to other compiler defaults.
      '/W3',
      # Use/enable ISO-standard C++ exception handling.
      '/EHsc',
      # Disable warnings about insecure CRT functions.  This is so that CL will
      # be more consistent with other compilers.
      '/D_CRT_SECURE_NO_WARNINGS',
      # The class has multiple copy constructors of a single type.  This is
      # disabled because there may be legitimate uses of multiple copy
      # constructors (e.g. one with a const parameter and one without).
      '/wd"4521"',
    ]

    if configuration.compile_for_shared_library:
      if configuration.optimize == 'Maximum':
        command += ['/LD']
      else:
        command += ['/LDd']
    else:
      if configuration.optimize == 'Maximum':
        command += ['/MT']
      else:
        command += ['/MTd']


    if configuration.include_symbols:
      command += ['/Z7']

    if configuration.optimize == 'Maximum':
      command += ['/O2', '/Ob2', '/Gy']
    else:
      command += ['/Od', '/Ob0']

    # Add configuration-based includes.
    for include_directory in configuration.include_directories:
      command += ['/I', include_directory]

    # Add configuration-based defines.
    command += ['/D' + x for x in configuration.defines]
    command += ['/D_SCL_SECURE_NO_WARNINGS']
    # Finally name the source and output files.
    command += [source_filepath, '/Fo:' + object_filepath]

    # Disable/Enable any explicitly toggled warnings.
    for k, e in configuration.warning_toggles.items():
      if k in _WARNING_TO_WARNING_NUMBER_MAP:
        warning_numbers = _WARNING_TO_WARNING_NUMBER_MAP[k]
        for warning_number in warning_numbers:
          if e:
            command += ['/w1' + warning_number]
          else:
            command += ['/wd' + warning_number]

    respire_deps_filepath = object_basepath + '.dr'

    # Wrap the call through a wrapper script to parse the resulting
    # /showIncludes output and forward on the actual output.
    wrapper_command = [
      sys.executable,
      self.deps_wrapper_script,
      respire_deps_filepath,
    ]
    # Add the system include directories.  These are added to the wrapper's
    # command line so that it can use them to avoid printing system include
    # directories.
    for include_directory in self.system_include_directories:
      wrapper_command += ['/I', include_directory]

    # Setup the actual compile command.
    registry.SystemCommand(
        inputs=[source_filepath] + extra_dependencies,
        outputs=[object_filepath],
        deps=respire_deps_filepath,
        command=wrapper_command + command)

    return object_filepath

  def ArchiveToStaticLibrary(self, registry, configuration, object_filepaths,
                             static_library_basepath):
    static_library_filepath = static_library_basepath + '.lib'

    command = [
      self.lib_executable_path,
      # Suppress the MSVC header/version information from being printed.
      # It's just noise.
      '/nologo',
      # Disable the "This object file does not define any previously undefined
      # public symbols, so it will not be used by any link operation that
      # consumes this library" warning, otherwise it is emitted on empty
      # modules.
      '/ignore:4221',
    ]

    command += object_filepaths

    command += ['/OUT:' + static_library_filepath]


    registry.SystemCommand(
        inputs=object_filepaths,
        outputs=[static_library_filepath],
        command=command)

    return static_library_filepath


  def _Link(self, registry, configuration, object_filepaths,
            static_library_filepaths, system_libraries,
            output_filepath, extra_link_flags, extra_input_files=[],
            output_lib_file=False):
    output_basepath = os.path.splitext(output_filepath)[0]

    command = [
      self.link_executable_path,
      # Suppress the MSVC header/version information from being printed.
      # It's just noise.
      '/nologo',
    ] + extra_link_flags

    if configuration.optimize == 'Maximum':
      command += ['/OPT:REF']

    pdb_filepath = None
    if configuration.include_symbols:
      command += ['/DEBUG']
      pdb_filepath = output_basepath + '.pdb'

    # Setup all the system library paths.
    for library_directory in self.system_library_directories:
      command += ['/LIBPATH:' + library_directory]

    # This was needed for a few functions used by Respire/Ebb.  There is
    # likely a better place to set this up, but I wasn't sure where just yet.
    command += ['shell32.lib'] + extra_input_files

    command += object_filepaths
    command += static_library_filepaths
    command += [x + '.lib' for x in system_libraries]

    command += ['/OUT:' + output_filepath]

    outputs = [output_filepath]
    soft_outputs = []
    if output_lib_file:
      # The lib file is only conditionally updated, so mark it as a soft
      # output.
      soft_outputs += [output_basepath + '.lib']
    if pdb_filepath:
      soft_outputs += [pdb_filepath]

    registry.SystemCommand(
        inputs=object_filepaths + static_library_filepaths,
        outputs=outputs,
        soft_outputs=soft_outputs,
        command=command)


    return outputs + soft_outputs


  def LinkToExecutable(self, registry, configuration, object_filepaths,
                       static_library_filepaths, system_libraries,
                       executable_basepath):
    return self._Link(registry, configuration, object_filepaths,
                      static_library_filepaths, system_libraries,
                      executable_basepath + '.exe', [
                        # By default link for a console entry point, to be
                        # consistent with other toolchains.
                        '/SubSystem:CONSOLE',
                      ])[0]


  def LinkToSharedLibrary(self, registry, configuration, object_filepaths,
                          static_library_filepaths, system_libraries,
                          shared_library_basepath):
    return self._Link(registry, configuration, object_filepaths,
                      static_library_filepaths, system_libraries,
                      shared_library_basepath + '.dll', [
                        # By default link for a console entry point, to be
                        # consistent with other toolchains.
                        '/DLL',
                      ],
                      extra_input_files=[
                        'kernel32.lib',
                        'user32.lib',
                      ],
                      output_lib_file=True)
