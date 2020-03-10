class Configuration(object):

  # List of warnings that can be explicitly enabled/disabled.
  WARNING_SIGNED_UNSIGNED_MISMATCH = 0
  WARNING_POSSIBLE_LOSS_OF_DATA_WHEN_CONVERTING = 1
  WARNING_NO_DEFINITION_FOR_INLINE_FUNCTION = 2
  WARNING_UNARY_MINUS_APPLIED_TO_UNSIGNED_TYPE = 3
  WARNING_SWITCH_STATEMENT_CONTAINS_ONLY_DEFAULT = 4
  WARNING_FORCING_VALUE_TO_BOOL = 5
  WARNING_UNKNOWN_ATTRIBUTE = 6
  WARNING_IGNORE_MISSING_OVERRIDE = 7


  def __init__(self, defines=[], optimize='None', include_symbols=True,
               compile_for_shared_library=False, include_directories=[],
               library_directories=[]):
    '''Creates a data object defining the compiler switches used for building.

       optimize: Can be 'None' or 'Maximum'
    '''
    self.defines = defines
    self.optimize = optimize
    self.include_symbols = include_symbols
    self.include_directories = include_directories
    self.library_directories = library_directories
    self.warning_toggles = {}
    self.compile_for_shared_library = compile_for_shared_library


class Toolchain(object):
  # Note that 'object_basepath' here refers to a file path for the output file
  # without an extension...  So that specific compilers can choose the
  # extension.
  def Compile(self, registry, configuration, source_filepath, object_basepath,
              extra_dependencies=[]):
    raise Exception('Method is not implemented for this toolchain.')

  def ArchiveToStaticLibrary(self, registry, configuration, object_filepaths,
                             static_library_basepath):
    raise Exception('Method is not implemented for this toolchain.')

  def LinkToSharedLibrary(self, registry, configuration, object_filepaths,
                          static_library_filepaths, system_libraries,
                          shared_library_basepath):
    raise Exception('Method is not implemented for this toolchain.')

  def LinkToExecutable(self, registry, configuration, object_filepaths,
                       static_library_filepaths, system_libraries,
                       executable_basepath):
    raise Exception('Method is not implemented for this toolchain.')


class ToolchainWithConfiguration(object):
  def __init__(self, toolchain, configuration):
    self.toolchain = toolchain
    self.configuration = configuration

  def Compile(self, registry, source_filepath, object_basepath,
              extra_dependencies=[]):
    return self.toolchain.Compile(registry, self.configuration, source_filepath,
                                  object_basepath, extra_dependencies)

  def ArchiveToStaticLibrary(self, registry, object_filepaths,
                             static_library_basepath):
    return self.toolchain.ArchiveToStaticLibrary(
        registry, self.configuration, object_filepaths, static_library_basepath)

  def LinkToSharedLibrary(self, registry, object_filepaths,
                          static_library_filepaths, system_libraries,
                          shared_library_basepath):
    return self.toolchain.LinkToSharedLibrary(
        registry, self.configuration, object_filepaths,
        static_library_filepaths, system_libraries, shared_library_basepath)

  def LinkToExecutable(self, registry, object_filepaths,
                       static_library_filepaths, system_libraries,
                       executable_basepath):
    return self.toolchain.LinkToExecutable(
        registry, self.configuration, object_filepaths,
        static_library_filepaths, system_libraries, executable_basepath)
