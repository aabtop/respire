from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import pprint


import log_output.output_to


class OutputToRaw(log_output.output_to.OutputTo):
  def __init__(self):
    self._pretty_printer = pprint.PrettyPrinter(indent=2)

  def OnEvent(self, event):
    self._pretty_printer.pprint(event)
