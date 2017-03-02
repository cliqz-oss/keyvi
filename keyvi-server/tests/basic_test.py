# -*- coding: utf-8 -*-
# Usage: py.test tests

from simple_test_server import tmp_server
import pykeyvi

def decode_to_unicode(data):
    """Recursively decodes byte strings to unicode"""
    if isinstance(data, bytes):
        return data.decode('utf-8')
    elif isinstance(data, dict):
        return dict((decode_to_unicode(k), decode_to_unicode(v))
                    for k, v in data.items())
    elif isinstance(data, list):
        return [decode_to_unicode(e) for e in data]
    return data


def test_set():
    with tmp_server() as t:
        writer = t.get_writer()
        writer.call('set', 'a', '{"b":3}')
        reader = t.get_reader()
        t.sync()
        x = reader.call('get', 'a')
        assert x is not None
        assert decode_to_unicode({"b": 3}) == decode_to_unicode(pykeyvi.Match.loads(x).GetValue())
        assert reader.call('exists', 'a')

def test_setnx():
    with tmp_server() as t:
        writer = t.get_writer()
        writer.call('set', 'a', '{"b":3}')
        t.sync()
        reader = t.get_reader()
        assert reader.call('exists', 'a')
        writer.call('setnx', 'a', '{"c":6}')
        writer.call('setnx', 'a', '{"d":2}')

        reader = t.get_reader()
        t.sync()
        x = reader.call('get', 'a')
        assert x is not None
        assert decode_to_unicode({"b": 3}) == decode_to_unicode(pykeyvi.Match.loads(x).GetValue())
        writer.call('commit', False)
        writer.call('setnx', 'a', '{"d":2}')
        assert x is not None
        assert decode_to_unicode({"b": 3}) == decode_to_unicode(pykeyvi.Match.loads(x).GetValue())
