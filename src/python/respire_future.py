class RespireFuture(object):
  def __init__(self, value_filepath, include_filepath, source_build_filepath,
               source_build_function):
    self.value_filepath = value_filepath
    self.include_filepath = include_filepath
    self.source_build_filepath = source_build_filepath
    self.source_build_function = source_build_function

  def ValueFilepath(self):
    return self.value_filepath

  def IncludeFilepath(self):
    return self.include_filepath

  def SourceBuildFilepath(self):
    return self.source_build_filepath

  def SourceBuildFunction(self):
    return self.source_build_function
