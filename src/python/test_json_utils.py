import inspect
import json_utils
from collections import namedtuple
import os
import sys
import unittest

THIS_MODULE_PATH = os.path.normcase(os.path.abspath(
                       inspect.getsourcefile(sys.modules[__name__])))

class Foo(object):
  def __init__(self, p):
    self.a = p
    self.b = 'foo'
    self.c = [1, 2, 3]
    self.d = {'blah': 'boo'}


Bar = namedtuple('Bar', 'do re mi fa')


class TestJsonUtils(unittest.TestCase):
  def test_SimpleObjectToDict(self):
    a = Foo(1)
    b = json_utils._ObjectToDict(a)

    self.assertEqual(b['vars']['a'], 1)
    self.assertEqual(b['vars']['b'], 'foo')
    self.assertEqual(b['vars']['c'], [1, 2, 3])
    self.assertEqual(b['vars']['d'], {'blah': 'boo'})

    self.assertEqual(b['module'], THIS_MODULE_PATH)
    self.assertEqual(b['class'], 'Foo')

  def test_NamedTupleObjectToDict(self):
    a = Bar(do=1, re='foo', mi=[1, 2, 3], fa={'hello': 'there'})
    b = json_utils._ObjectToDict(a)

    self.assertEqual(b['vars']['do'], 1)
    self.assertEqual(b['vars']['re'], 'foo')
    self.assertEqual(b['vars']['mi'], [1, 2, 3])
    self.assertEqual(b['vars']['fa'], {'hello': 'there'})

    self.assertEqual(b['module'], THIS_MODULE_PATH)
    self.assertEqual(b['class'], 'Bar')

  def test_SimpleDictToObject(self):
    a = Foo(1)
    b = json_utils._ObjectToDict(a)
    c = json_utils._DictToObject(b)

    self.assertTrue(isinstance(c, Foo))
    self.assertEqual(c.a, 1)
    self.assertEqual(c.b, 'foo')
    self.assertEqual(c.c, [1, 2, 3])
    self.assertEqual(c.d, {'blah': 'boo'})

  def test_NamedTupleDictToObject(self):
    a = Bar(do=1, re='foo', mi=[1, 2, 3], fa={'hello': 'there'})
    b = json_utils._ObjectToDict(a)
    c = json_utils._DictToObject(b)

    self.assertTrue(isinstance(c, Bar))
    self.assertEqual(c.do, 1)
    self.assertEqual(c.re, 'foo')
    self.assertEqual(c.mi, [1, 2, 3])
    self.assertEqual(c.fa, {'hello': 'there'})

  def test_EncodeDecodeObjectToJSON(self):
    a = Foo(1)
    b = json_utils.EncodeToJSON(a)[0]
    c = json_utils.DecodeFromJSON(b)

    self.assertTrue(isinstance(c, Foo))
    self.assertEqual(c.a, 1)
    self.assertEqual(c.b, 'foo')
    self.assertEqual(c.c, [1, 2, 3])
    self.assertEqual(c.d, {'blah': 'boo'})

  def test_EncodeDecodeNestedObjectToJSON(self):
    a = Foo(1)
    b = Foo(a)
    c = json_utils.EncodeToJSON(b)[0]
    d = json_utils.DecodeFromJSON(c)

    self.assertTrue(isinstance(d, Foo))
    self.assertTrue(isinstance(d.a, Foo))
    self.assertEqual(d.a.a, 1)
    self.assertEqual(d.a.b, 'foo')
    self.assertEqual(d.a.c, [1, 2, 3])
    self.assertEqual(d.a.d, {'blah': 'boo'})
    self.assertEqual(d.b, 'foo')
    self.assertEqual(d.c, [1, 2, 3])
    self.assertEqual(d.d, {'blah': 'boo'})

  def test_EncodeDecodeTupleToJSON(self):
    a = Bar(do=1, re='foo', mi=[1, 2, 3], fa={'hello': 'there'})
    b = json_utils.EncodeToJSON(a)[0]
    c = json_utils.DecodeFromJSON(b)

    self.assertTrue(isinstance(c, Bar))
    self.assertEqual(c.do, 1)
    self.assertEqual(c.re, 'foo')
    self.assertEqual(c.mi, [1, 2, 3])
    self.assertEqual(c.fa, {'hello': 'there'})

  def test_EncodeDecodeWithFlatteningObjectToJSON(self):
    a = Foo(1)
    b = json_utils.EncodeToJSON(a)[0]
    c = json_utils.DecodeFromJSONWithFlattening(b)

    self.assertTrue(isinstance(c, Foo))
    self.assertEqual(c.a, 1)
    self.assertEqual(c.b, 'foo')
    self.assertEqual(c.c, [1, 2, 3])
    self.assertEqual(c.d, {'blah': 'boo'})
