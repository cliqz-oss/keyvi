# -*- coding: utf-8 -*-
# Usage: py.test tests

from simple_test_server import tmp_server
import pykeyvi

def test_set():
    with tmp_server() as t:
        writer = t.get_writer()
        writer.call('set', 'a', '{"b":3}')
        reader = t.get_reader()
        t.sync()
        x = reader.call('get', 'a')
        assert x is not None
        assert {"b": 3} == pykeyvi.Match.loads(x).GetValue()
        assert reader.call('exists', 'a')

def test_setnx():
    with tmp_server() as t:
        writer = t.get_writer()
        writer.call('set', 'a', '{"b":3}')
        writer.call('commit', False)
        writer.call('setnx', 'a', '{"c":6}')
        writer.call('setnx', 'a', '{"d":2}')

        reader = t.get_reader()
        t.sync()
        x = reader.call('get', 'a')
        assert x is not None
        assert {"b": 3} == pykeyvi.Match.loads(x).GetValue()
        writer.call('commit', False)
        writer.call('setnx', 'a', '{"d":2}')
        assert x is not None
        assert {"b": 3} == pykeyvi.Match.loads(x).GetValue()
