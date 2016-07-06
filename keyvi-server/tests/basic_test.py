# -*- coding: utf-8 -*-
# Usage: py.test tests

from simple_test_server import tmp_server
import time

def test_set():
    with tmp_server() as t:
        writer = t.get_writer()
        writer.call('set', 'a', '{"b":3}')
        reader = t.get_reader()
        t.sync()
        x = reader.call('get', 'a')
        assert x is not None