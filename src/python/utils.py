import platform


def IsString(obj):
  if platform.python_version_tuple()[0] == '2':
    return isinstance(obj, basestring)
  else:
    return isinstance(obj, str)
