import collections
import platform
import os
import shutil

import respire.buildlib.touch as touch
import respire.buildlib.utils as utils


def CopyFiles(registry, inputs, output_dir, stamp_file=None, filter=None):
  absolute_inputs = (
      [utils.ResolveFilepathRelativeToCallerScript(x) for x in inputs])
  
  _DoCopyFiles(registry=registry, inputs=absolute_inputs,
               output_dir=output_dir, stamp_file=stamp_file,
               filter=filter)


# Copies entire directories, but also it must wait for the directory
# to be created before it can check the individual files inside of it to
# copy.  The reason we need a distinct function for this is because when
# the directory (or file) is generated, we don't know ahead of time whether
# it's a directory or a file.
def CopyGeneratedDirectories(
    registry, inputs, output_dir, stamp_file=None, filter=None):
  absolute_inputs = (
      [utils.ResolveFilepathRelativeToCallerScript(x) for x in inputs])
  
  registry.SubRespire(_DoCopyFiles, additional_deps=absolute_inputs,
                      inputs=absolute_inputs, output_dir=output_dir,
                      stamp_file=stamp_file, filter=filter)


def _DoCopyFiles(registry, inputs, output_dir, stamp_file, filter):
  source_files_by_out_directory = collections.defaultdict(list)

  for input_path in inputs:
    if not os.path.isdir(input_path):
      if filter and not filter(input_path):
        continue
      source_files_by_out_directory[os.curdir].append(input_path)
    else:
      # We record the input_path's containing directory here so that we
      # can include the |input_path| itself in the output directory path.
      containing_directory = os.path.dirname(input_path)
      for root, dirnames, filenames in os.walk(input_path):
        registry.RegisterSelfDependency(root)
        rel_root = os.path.relpath(root, containing_directory)
        for filename in filenames:
          full_path = os.path.normpath(os.path.join(root, filename))
          if filter and not filter(full_path):
            continue
          source_files_by_out_directory[rel_root].append(full_path)

  # Build a total list of all input files.
  all_output_files = []

  # Now that we have the list of source files to copy, and their relative
  # destination within the output folder, setup the actual single-file copies.
  for rel_dir, input_files in source_files_by_out_directory.items():
    output_sub_dir = os.path.join(output_dir, rel_dir)
    _CreateDirectory(output_sub_dir)
    for input_file in input_files:
      output_file = os.path.abspath(
          os.path.join(output_sub_dir, os.path.basename(input_file)))
      SingleFileCopy(registry, input_file, output_file)
      all_output_files.append(output_file)

  if stamp_file:
    registry.PythonFunction(
        inputs=all_output_files, outputs=[stamp_file], function=touch.Touch)


if platform.system() == 'Windows':
  WINDOWS_COPY_COMMAND = (
      os.path.join(os.path.dirname(__file__), 'windows_utils', 'copy.cmd'))


def SingleFileCopy(registry, input_file, output_file):
  if platform.system() == 'Windows':
    registry.SystemCommand(
        inputs=[input_file],
        outputs=[output_file],
        command=[WINDOWS_COPY_COMMAND, input_file, output_file])
  else:
    registry.SystemCommand(
        inputs=[input_file],
        outputs=[output_file],
        command=['cp', input_file, output_file])


def _CreateDirectory(directory):
  if not os.path.exists(directory):
    os.makedirs(directory)
  else:
    if not os.path.isdir(directory):
      raise Exception('A non-directory file already exists where we were '
                      'requested to make a directory.')

def _DoCopy(inputs, outputs):
  shutil.copyfile(inputs[0], outputs[0])
  return 0