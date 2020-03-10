from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import json
import platform
import utils


def CheckListOfStrings(obj):
  if not isinstance(obj, (list,)):
    raise Exception(
        'Expected a list of strings, instead got a: ' + str(type(obj)))
  for x in obj:
    if not utils.IsString(x):
      raise Exception('Expected a list of strings, but one of its items is '
                      'not a string, it is of type: '
                      + str(type(x)) + '.')


def CheckString(obj):
  if not utils.IsString(obj):
    raise Exception('Expected a string, found item with type: '
                    + str(type(obj)) + '.')


def _ConvertToShellCommandString(command):
  '''Converts a list of command line tokens into a string for the shell.'''
  def _AddQuotesIfStringContainsSpaces(token):
    if ' ' in token:
      return '"' + token + '"'
    else:
      return token

  return ' '.join([_AddQuotesIfStringContainsSpaces(x) for x in command])


class _SystemCommand(object):
  def __init__(self, inputs, outputs, command, soft_outputs, deps, stdout,
               stderr, stdin):
    CheckListOfStrings(inputs)
    CheckListOfStrings(outputs)
    if soft_outputs:
      CheckListOfStrings(soft_outputs)
    if deps:
      CheckString(deps)
    if stdout:
      CheckString(stdout)
    if stderr:
      CheckString(stderr)
    if stdin:
      CheckString(stdin)
    CheckListOfStrings(command)

    self.inputs = inputs
    self.outputs = outputs
    self.soft_outputs = soft_outputs
    self.command = command
    self.deps = deps
    self.stdout = stdout
    self.stderr = stderr
    self.stdin = stdin


class _Include(object):
  def __init__(self, path):
    self.path = path


class _Build(object):
  def __init__(self, path):
    self.path = path


class RespireBuilder(object):
  def __init__(self):
    self.pending_entries = []

  def AddSystemCommand(self, inputs, outputs, command, soft_outputs=None,
                       deps=None, stdout=None, stderr=None, stdin=None):
    self.pending_entries.append(_SystemCommand(
        inputs, outputs, command, soft_outputs, deps, stdout, stderr, stdin))

  def AddInclude(self, include_file_path):
    self.pending_entries.append(_Include(include_file_path))

  def AddBuild(self, target):
    self.pending_entries.append(_Build(target))

  def _CompileToJSON(self):
    class TypedEntryList(object):
      def __init__(self):
        self.root_list = []
        self._current_type = None
        self._current_typed_entry_list = None

      def AppendCurrentEntryList(self):
        if not self._current_typed_entry_list:
          return

        if self._current_type.__name__ == '_SystemCommand':
          type_key = 'sc'
        elif self._current_type.__name__ == '_Include':
          type_key = 'inc'
        elif self._current_type.__name__ == '_Build':
          type_key = 'build'

        self.root_list.append({type_key: self._current_typed_entry_list})
        self._current_typed_entry_list = None
        self._current_type = None

      def AppendEntry(self, entry):
        entry_type = type(entry)
        if entry_type != self._current_type:
          self.AppendCurrentEntryList()
          self._current_typed_entry_list = []
          self._current_type = entry_type

        if entry_type.__name__ == '_SystemCommand':
          system_command_entry = {
            'in': entry.inputs,
            'out': entry.outputs,
            'cmd': _ConvertToShellCommandString(entry.command),
          }
          if entry.soft_outputs:
            system_command_entry['soft_out'] = entry.soft_outputs
          if entry.deps:
            system_command_entry['deps'] = entry.deps
          if entry.stdout:
            system_command_entry['stdout'] = entry.stdout
          if entry.stderr:
            system_command_entry['stderr'] = entry.stderr
          if entry.stdin:
            system_command_entry['stdin'] = entry.stdin


          self._current_typed_entry_list.append(system_command_entry)
        elif entry_type.__name__ == '_Include':
          self._current_typed_entry_list.append(entry.path)
        elif entry_type.__name__ == '_Build':
          self._current_typed_entry_list.append(entry.path)


    entry_list = TypedEntryList()

    for entry in self.pending_entries:
      entry_list.AppendEntry(entry)

    entry_list.AppendCurrentEntryList()

    return entry_list.root_list


  def CompileToString(self):
    return json.dumps(
        self._CompileToJSON(), indent=2, separators=(',', ': '))


  def CompileToFile(self, out_respire_file):
    out_directory = os.path.dirname(out_respire_file)
    if not os.path.exists(out_directory):
      os.makedirs(out_directory)

    file_contents = self.CompileToString()

    with open(out_respire_file, 'w') as f:
      f.write(file_contents)
