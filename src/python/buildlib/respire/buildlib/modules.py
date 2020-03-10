import copy
import inspect
import os
import sys

import respire.buildlib.cc as cc
import respire.buildlib.copy_files as copy_files
import respire.buildlib.utils as utils
import respire.buildlib.touch as touch


class Module(object):
  def __init__(self, name, module_dependencies=[]):
    self.name = name
    self.module_dependencies = module_dependencies
    self.custom_dependency_hooks = []
    self.package_files = []
    self.package_generated_directories = []    
    for module_dependency in module_dependencies:
      assert module_dependency

  # Sets a custom dependency hook that will be called on every depender that
  # depends on this module.
  def AddCustomDependencyHook(self, function):
    utils.AssertIsValidRespireFunction(function)
    self.custom_dependency_hooks.append(function)

  # Called by a subclass to execute the process of calling all of this module's
  # dependency's dependency hooks on us, the depender.
  def ProcessDependencyHooks(self, registry):
    for dependency in self.module_dependencies:
      dependency.DependencyHook(self, registry)
      for custom_hook in dependency.custom_dependency_hooks:
        custom_hook(self, dependency, registry)

  # To be implemented by subclasses.  Called by a depender's
  # ProcessDependencyHooks() call.  It does not need to be implemented.
  def DependencyHook(self, depender, registry):
    self._PackageDependencyHook(depender, registry)

  def GetOutputFiles(self):
    return []

  # Attach a package file to this module, which will be propagated to its
  # dependers.  When CopyPackageFiles() is called on a module, all of its
  # package files as well as its main output file are copied to the specified
  # directory.
  def AttachPackageFile(self, src):
    absolute_src = utils.ResolveFilepathRelativeToCallerScript(src)
    self.package_files.append(absolute_src)

  # Similar to the above AttachPackageFile(), but with directories, we must
  # wait for the directory to exist before we can evaluate the files within
  # it that need to be copied.
  def AttachPackageGeneratedDirectory(self, src):
    absolute_src = utils.ResolveFilepathRelativeToCallerScript(src)
    self.package_generated_directories.append(absolute_src)

  def CopyPackageFiles(self, out_dir, registry, copy_stamp_file=None):
    if not copy_stamp_file:
      copy_stamp_file = os.path.join(out_dir, 'package_files_copied.stamp')

    sub_stamps = []
    if self.package_files:
      files_to_copy = self.package_files
      output_files = self.GetOutputFiles()
      if output_files:
        files_to_copy += [output_files[0]]

      files_stamp_file = os.path.join(
                             out_dir, '_package_files_files_copied.stamp')
      copy_files.CopyFiles(
          registry,
          inputs=files_to_copy,
          output_dir=out_dir,
          stamp_file=files_stamp_file)

      sub_stamps.append(files_stamp_file)

    if self.package_generated_directories:
      directories_stamp_file = os.path.join(
          out_dir, '_package_files_generated_directories_copied.stamp')
      copy_files.CopyGeneratedDirectories(
          registry,
          inputs=self.package_generated_directories,
          output_dir=out_dir,
          stamp_file=directories_stamp_file)

      sub_stamps.append(directories_stamp_file)

    registry.PythonFunction(
      inputs=sub_stamps,
      outputs=[copy_stamp_file],
      function=touch.Touch)
    
    return copy_stamp_file

  # Opportunity to remove attributes that don't need to be read by depending
  # modules...  Which can considerably cut down on bytes required to pass
  # this object around.
  def _RemoveNonTransitiveAttributes(self):
    del self.module_dependencies

  def _PackageDependencyHook(self, depender, registry):
    # Propagate package files up the module tree.
    depender.package_files = list(set(
        depender.package_files + self.package_files))
    depender.package_generated_directories = list(set(
        depender.package_generated_directories +
            self.package_generated_directories))



def _CompileSourcesToObjects(registry, out_dir, configured_toolchain, sources,
                             inherited_hard_dependencies, is_shared_library):
  if is_shared_library:
    # Enable shared library sources to be compiled with special flags.
    modified_configured_toolchain = copy.deepcopy(configured_toolchain)
    modified_configured_toolchain.configuration.compile_for_shared_library = (
        True)
    configured_toolchain = modified_configured_toolchain


  object_filepaths = []
  for source in sources:
    source_split = os.path.splitext(os.path.basename(source))

    if source_split[1] == '.h':
        # Ignore header files.
        continue

    object_filepath = configured_toolchain.Compile(
        registry, source, os.path.join(out_dir, source_split[0]),
        extra_dependencies=inherited_hard_dependencies)
    object_filepaths.append(object_filepath)
  return object_filepaths


class CCModule(Module):
  def __init__(self, name, registry, out_dir, configured_toolchain, sources=[],
               objects=[], static_libraries=[], system_libraries=[],
               inherited_hard_dependencies=[], module_dependencies=[],
               is_shared_library=False):
    super(CCModule, self).__init__(name, module_dependencies)
    self.configured_toolchain = configured_toolchain
    self.sources = (
        [utils.ResolveFilepathRelativeToCallerScript(x) for x in sources])
    self.objects = (
        [utils.ResolveFilepathRelativeToCallerScript(x) for x in objects])
    self.static_libraries = (
        [utils.ResolveFilepathRelativeToCallerScript(x) for x in
             static_libraries])
    self.system_libraries = system_libraries
    self.inherited_hard_dependencies = (
        [utils.ResolveFilepathRelativeToCallerScript(x) for x in
             inherited_hard_dependencies])
    self.out_dir = out_dir
    self.shared_stub_libraries = []

    self.ProcessDependencyHooks(registry)

    self.objects += _CompileSourcesToObjects(
        registry, out_dir, configured_toolchain, self.sources,
        self.inherited_hard_dependencies, is_shared_library=is_shared_library)

  def DependencyHook(self, depender, registry):
    super(CCModule, self).DependencyHook(depender, registry)
    # Inherit the inherited hard dependencies.
    depender.inherited_hard_dependencies += self.inherited_hard_dependencies

  def _RemoveNonTransitiveAttributes(self):
    super(CCModule, self)._RemoveNonTransitiveAttributes()
    del self.sources
    del self.objects
    del self.configured_toolchain


class ExecutableModule(CCModule):
  def __init__(self, name, registry, out_dir, configured_toolchain, sources=[],
               objects=[], static_libraries=[], system_libraries=[],
               inherited_hard_dependencies=[], module_dependencies=[]):
    super(ExecutableModule, self).__init__(
        name, registry, out_dir, configured_toolchain, sources, objects,
        static_libraries, system_libraries, inherited_hard_dependencies,
        module_dependencies)

    static_libraries = self.static_libraries + self.shared_stub_libraries

    self.output_filepath = configured_toolchain.LinkToExecutable(
        registry, self.objects, static_libraries, self.system_libraries,
        os.path.join(out_dir, self.name))

    self._RemoveNonTransitiveAttributes()

  def GetOutputFiles(self):
    return [self.output_filepath]


class SharedLibraryModule(CCModule):
  def __init__(self, name, registry, out_dir, configured_toolchain, sources=[],
               objects=[], static_libraries=[], system_libraries=[],
               public_include_paths=[], inherited_hard_dependencies=[],
               module_dependencies=[]):
    self.public_include_paths = (
        [utils.ResolveFilepathRelativeToCallerScript(x)
             for x in public_include_paths])
    configured_toolchain.configuration.include_directories = list(set(
        configured_toolchain.configuration.include_directories +
            self.public_include_paths))
    super(SharedLibraryModule, self).__init__(
        name, registry, out_dir, configured_toolchain, sources, objects,
        static_libraries, system_libraries, inherited_hard_dependencies,
        module_dependencies, is_shared_library=True)

    static_libraries = self.static_libraries + self.shared_stub_libraries

    shared_library_output = (
        configured_toolchain.LinkToSharedLibrary(
            registry, self.objects, static_libraries,
            self.system_libraries, os.path.join(out_dir, self.name)))

    self.output_files = [shared_library_output[0]]
    self.shared_stub_libraries.append(shared_library_output[1])

    self._RemoveNonTransitiveAttributes()

  def DependencyHook(self, depender, registry):
    super(SharedLibraryModule, self).DependencyHook(depender, registry)

    # Propagate all our static library dependencies to our dependers since
    # we don't actually link any of them while archiving.  And of course, we
    # need to add ourselves to the depender's static libraries set.
    if isinstance(depender, CCModule):
      for shared_stub_library in self.shared_stub_libraries:
        if shared_stub_library not in depender.shared_stub_libraries:
          depender.shared_stub_libraries.append(shared_stub_library)

      depender.configured_toolchain.configuration.include_directories += (
          self.public_include_paths)
      if not isinstance(depender, ExecutableModule):
        # Propagate the public includes.  This may not actually be what we want
        # to do here, but it's a quick and easy solution for now.
        depender.public_include_paths += self.public_include_paths

    depender.AttachPackageFile(self.output_files[0])


  def GetOutputFiles(self):
    return self.output_files


class StaticLibraryModule(CCModule):
  def __init__(self, name, registry, out_dir, configured_toolchain, sources=[],
               objects=[], static_libraries=[], system_libraries=[],
               public_include_paths=[], inherited_hard_dependencies=[],
               module_dependencies=[]):
    self.public_include_paths = (
        [utils.ResolveFilepathRelativeToCallerScript(x)
             for x in public_include_paths])
    configured_toolchain.configuration.include_directories = list(set(
        configured_toolchain.configuration.include_directories +
            self.public_include_paths))
    super(StaticLibraryModule, self).__init__(
        name, registry, out_dir, configured_toolchain, sources, objects,
        static_libraries, system_libraries, inherited_hard_dependencies,
        module_dependencies)

    if self.objects:
      self.output_filepath = configured_toolchain.ArchiveToStaticLibrary(
          registry, self.objects, os.path.join(out_dir, self.name))

    self._RemoveNonTransitiveAttributes()

  def DependencyHook(self, depender, registry):
    super(StaticLibraryModule, self).DependencyHook(depender, registry)

    # Propagate all our static library dependencies to our dependers since
    # we don't actually link any of them while archiving.  And of course, we
    # need to add ourselves to the depender's static libraries set.
    if isinstance(depender, CCModule):
      if (hasattr(self, 'output_filepath') and
          self.output_filepath not in depender.static_libraries):
        depender.static_libraries.append(self.output_filepath)
      for static_library in self.static_libraries:
        if static_library not in depender.static_libraries:
          depender.static_libraries.append(static_library)
      for shared_stub_library in self.shared_stub_libraries:
        if shared_stub_library not in depender.shared_stub_libraries:
          depender.shared_stub_libraries.append(shared_stub_library)
      for system_library in self.system_libraries:
        if system_library not in depender.system_libraries:
          depender.system_libraries.append(system_library)
      depender.configured_toolchain.configuration.include_directories += (
          self.public_include_paths)
      if not isinstance(depender, ExecutableModule):
        # Propagate the public includes.  This may not actually be what we want
        # to do here, but it's a quick and easy solution for now.
        depender.public_include_paths += self.public_include_paths

  def GetOutputFiles(self):
    if hasattr(self, 'output_filepath'):
      return [self.output_filepath]
    else:
      return []
