# -*- coding: utf-8 -*-
# Usage: py.test tests

import contextlib
import os

import pykeyvi

import sys
import os

root = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(root, "../"))

from test_tools import tmp_dictionary

def test_simple():
    c = pykeyvi.JsonDictionaryCompiler()
    c.Add("abc", '{"a" : 2}')
    c.Add("abd", '{"a" : 3}')
    # use python syntax ala __setitem__
    c["abd"] = '{"a" : 3}'
    with tmp_dictionary(c, 'simple_json.kv') as d:
        assert len(d) == 2
        assert d["abc"].GetValueAsString() == '{"a":2}'
        assert d["abd"].GetValueAsString() == '{"a":3}'
        assert len([(k, v) for k, v in d.GetAllItems()]) == 2

def test_simple_zlib():
    c = pykeyvi.JsonDictionaryCompiler(50000000, {'compression': 'z', 'compression_threshold': '0'})
    c.Add("abc", '{"a" : 2}')
    c.Add("abd", '{"a" : 3}')
    with tmp_dictionary(c, 'simple_json_z.kv') as d:
        assert len(d) == 2
        assert d["abc"].GetValueAsString() == '{"a":2}'
        assert d["abd"].GetValueAsString() == '{"a":3}'
        m = d.GetStatistics()['Value Store']
        assert m['__compression'] == "zlib"
        assert m['__compression_threshold'] == "0"

def test_simple_snappy():
    c = pykeyvi.JsonDictionaryCompiler(50000000, {'compression': 'snappy', 'compression_threshold': '0'})
    c.Add("abc", '{"a" : 2}')
    c.Add("abd", '{"a" : 3}')
    with tmp_dictionary(c, 'simple_json_snappy.kv') as d:
        assert len(d) == 2
        assert d["abc"].GetValueAsString() == '{"a":2}'
        assert d["abd"].GetValueAsString() == '{"a":3}'
        m = d.GetStatistics()['Value Store']
        assert m['__compression'] == "snappy"
        assert m['__compression_threshold'] == "0"

def test_unicode_compile():
    c = pykeyvi.JsonDictionaryCompiler()
    c.Add("üöä", '{"y" : 2}')
    c.Add("üüüüüüabd".decode('utf-8'), '{"a" : 3}')
    c.Add(u"ääääädäd", '{"b" : 33}')

    with tmp_dictionary(c, 'simple_json.kv') as d:
        assert len(d) == 3
        assert d["üöä"].GetValueAsString() == '{"y":2}'
        assert d[u"üöä"].GetValueAsString() == '{"y":2}'
        assert d["üüüüüüabd"].GetValueAsString() == '{"a":3}'
        assert d["ääääädäd"].GetValueAsString() == '{"b":33}'


