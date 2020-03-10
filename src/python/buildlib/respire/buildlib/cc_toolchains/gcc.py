import os
import sys

import respire.buildlib.cc as cc

def ReplacePathBasenameSuffixWith(path, replacee, replacer):
  splitext_path = os.path.splitext(path)
  replaced_basepath = splitext_path[0]
  if splitext_path[0].endswith(replacee):
    replaced_basepath = splitext_path[0][:-len(replacee)] + replacer
    return os.path.join(replaced_basepath + splitext_path[1])
  else:
    return None

def DeriveGppPathFromGccPath(gcc_path):
  gpp_path = ReplacePathBasenameSuffixWith(gcc_path, 'gcc', 'g++')
  assert gpp_path
  return gpp_path

def DeriveArPathFromGccPath(gcc_path):
  ar_path = ReplacePathBasenameSuffixWith(gcc_path, 'gcc', 'ar')
  assert ar_path
  return ar_path


_WARNING_TO_WARNING_SWITCH_MAP = {
  cc.Configuration.WARNING_UNKNOWN_ATTRIBUTE: ['attributes'],
  cc.Configuration.WARNING_IGNORE_MISSING_OVERRIDE: [
      'inconsistent-missing-override'
  ],
}


class GccToolchain(cc.Toolchain):
  def __init__(
      self, gcc_executable_path,
      gpp_executable_path=None,
      ar_executable_path=None):
    if not gpp_executable_path:
      gpp_executable_path = DeriveGppPathFromGccPath(gcc_executable_path)
    if not ar_executable_path:
      ar_executable_path = DeriveArPathFromGccPath(gcc_executable_path)
    self.gcc_executable_path = gcc_executable_path
    self.gpp_executable_path = gpp_executable_path
    self.ar_executable_path = ar_executable_path
    self.ar_wrapper_script_path = (
        os.path.join(os.path.dirname(__file__), 'wrap_ar_to_delete_first.py'))
    self.toolchain_include_directories = []
    self.toolchain_library_directories = []

  def Compile(self, registry, configuration, source_filepath, object_basepath,
              extra_dependencies=[]):
    command = [self.gpp_executable_path]

    source_ext = os.path.splitext(source_filepath)[1]
    if source_ext == '.c':
      # This is a C file.
      command += [
        '-x', 'c',
        '-std=c11',
      ]
    else:
      # This is a C++ file.
      command += [
      # Use C++ 11 by default.
      '-std=c++11',
    ]

    for include_directory in (
        self.toolchain_include_directories + configuration.include_directories):
      command += ['-I',  include_directory]

    # Add configuration-based defines.
    command += ['-D' + x for x in configuration.defines]

    command += [
      # We don't always know in advance if we'll end up if a given source
      # module will get linked into a shared library or not, so we cautiously
      # compile all code as position independent.
      '-fpic',
      # Enables dead code stripping and also is what we want as a default
      # for if we're compiling for a shared library.
      '-fvisibility=hidden',
    ]

    if configuration.include_symbols:
      command += ['-g']
    if configuration.optimize == 'Maximum':
      command += ['-O2']
      # To enable dead code stripping by the linker.
      command += ['-ffunction-sections', '-fdata-sections']

    object_filepath = object_basepath + '.o'
    make_deps_file = object_basepath + '.d'
    command += ['-MMD', '-MF', make_deps_file]

    # Disable/Enable any explicitly toggled warnings.
    for k, e in configuration.warning_toggles.items():
      if k in _WARNING_TO_WARNING_SWITCH_MAP:
        warning_switches = _WARNING_TO_WARNING_SWITCH_MAP[k]
        for warning_switch in warning_switches:
          if e:
            command += ['-W' + warning_switch]
          else:
            command += ['-Wno-' + warning_switch]
    # We use some warnings that aren't available on all versions of gcc/clang,
    # so silence the warnings about these when using older versions.
    command += ['-Wno-unknown-warning-option']

    if hasattr(self, 'additional_compiler_flags'):
      command += self.additional_compiler_flags

    command += ['-c', source_filepath, '-o', object_filepath]

    respire_deps_file = object_basepath + '.dr'
    deps_convert_script = os.path.join(os.path.dirname(__file__),
                                       'deps_make_to_respire.py')

    # Setup a command for converting between gcc's makefile-based deps file
    # to a respire formatted deps file.
    registry.SystemCommand(
        inputs=[make_deps_file],
        outputs=[respire_deps_file],
        command=[sys.executable, deps_convert_script, '-i', make_deps_file,
                 '-o', respire_deps_file])

    # Setup the actual compile command.
    registry.SystemCommand(
        inputs=[source_filepath] + extra_dependencies,
        outputs=[object_filepath],
        deps=respire_deps_file,
        command=command)

    return object_filepath

  def ArchiveToStaticLibrary(self, registry, configuration, object_filepaths,
                             static_library_basepath):
    static_library_filepath = static_library_basepath + '.a'
    command = ([sys.executable, self.ar_wrapper_script_path,
                self.ar_executable_path, 'rcs', static_library_filepath] +
                object_filepaths)

    registry.SystemCommand(
        inputs=object_filepaths,
        outputs=[static_library_filepath],
        command=command)

    return static_library_filepath

  def _Link(self, registry, configuration, object_filepaths,
                       static_library_filepaths, system_libraries,
                       output_path, extra_flags=[]):
    command = ([self.gpp_executable_path] +
               extra_flags +
               ['-o', output_path] +
               object_filepaths)

    command += ['-std=c++11', '-lpthread']
    if configuration.optimize:
      command += ['-Wl,--gc-sections']

    command += ['-Wl,--exclude-libs,ALL']

    if hasattr(self, 'additional_linker_flags'):
      command += self.additional_linker_flags

    command += list(set(
        ['-L' + os.path.dirname(x) for x in static_library_filepaths]))

    command += ['-l:' + os.path.split(x)[1] for x in static_library_filepaths]

    command += ['-L' + x for x in (
        self.toolchain_library_directories + configuration.library_directories)
    ]

    command += ['-Wl,-rpath-link,' + x for x in (
                self.toolchain_library_directories)]

    command += ['-l' + x for x in system_libraries]

    registry.SystemCommand(
        inputs=object_filepaths + static_library_filepaths,
        outputs=[output_path],
        command=command)

    return output_path

  def LinkToExecutable(self, registry, configuration, object_filepaths,
                       static_library_filepaths, system_libraries,
                       executable_basepath):
    return self._Link(
        registry, configuration, object_filepaths, static_library_filepaths,
        system_libraries, executable_basepath,
        extra_flags=[
          # This flag is required to tell the elf runtime linker to search for
          # shared object files in the same directory as the executable.
          r'-Wl,-rpath,"\$ORIGIN"',
        ])

  def LinkToSharedLibrary(self, registry, configuration, object_filepaths,
                          static_library_filepaths, system_libraries,
                          shared_library_basepath):
    shared_library_path = shared_library_basepath + '.so'
    output_file = self._Link(
        registry, configuration, object_filepaths, static_library_filepaths,
        system_libraries, shared_library_path,
        extra_flags=[
          # Tell the linker to compile into a shared library.
          '-shared',
          # Make it so that the runtime linker will search for the shared
          # library in the same directory as the executable.
          '-Wl,-soname,' + os.path.split(shared_library_path)[1],
        ])

    # The module framework expects the 
    return [shared_library_path, shared_library_path]
