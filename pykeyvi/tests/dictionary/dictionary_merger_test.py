# -*- coding: utf-8 -*-
# Usage: py.test tests

import pykeyvi

import sys
import os
import json
import tempfile
import shutil
import collections
import pytest

from os import path

root = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(root, "../"))

from test_tools import decode_to_unicode

key_values_1 = {
    'a' : {"a": 2},
    'bzzzz' : {"b": 2},
    'bbb' : {"b": 2},
    'b' : {"b": 2}
}

key_values_2 = {
    'a' : {"ab": 23},
    'c' : {"c": 2},
    'i' : {"c": 2},
    'ia' : {"c": 2},
    'd' : {"d": 2}
}

key_values_3 = {
    'e' : {"e": 2},
    'd' : {"dddd": 1233},
    'e1' : {"e": 2},
    'e2' : {"e": 22},
    'e3' : {"e": 21},
    'e4' : {"e": 2},
    'f' : {"f": 2}
}

def generate_keyvi(key_values, filename):

    dictionary_compiler = pykeyvi.JsonDictionaryCompiler({"memory_limit_mb":"10"})
    for key, value in key_values.items():
        dictionary_compiler.Add(key, json.dumps(value))

    dictionary_compiler.Compile()
    dictionary_compiler.WriteToFile(filename)

@pytest.mark.parametrize('merger', [pykeyvi.JsonDictionaryMerger({"memory_limit_mb":"10"}),
                                    pykeyvi.JsonDictionaryMerger({"memory_limit_mb":"10", 'merge_mode': 'append'})])
def test_merge(merger):

    tmp_dir = tempfile.mkdtemp()
    try:
        file_1 = path.join(tmp_dir, 'test_merger_1.kv')
        file_2 = path.join(tmp_dir, 'test_merger_2.kv')
        file_3 = path.join(tmp_dir, 'test_merger_3.kv')
        merge_file = path.join(tmp_dir, 'merge.kv')

        generate_keyvi(key_values_1, file_1)
        generate_keyvi(key_values_2, file_2)
        generate_keyvi(key_values_3, file_3)


        merger.Add(file_1)
        merger.Add(file_2)
        merger.Add(file_3)
        merger.Merge(merge_file)


        merged_dictionary = pykeyvi.Dictionary(merge_file)

        key_values = {}
        key_values.update(key_values_1)
        key_values.update(key_values_2)
        key_values.update(key_values_3)

        key_values_ordered = collections.OrderedDict(sorted(key_values.items()))

        for (base_key, base_value), (keyvi_key, keyvi_value) in zip(key_values_ordered.items(), merged_dictionary.GetAllItems()):
            assert decode_to_unicode(base_key) == decode_to_unicode(keyvi_key)
            assert decode_to_unicode(base_value) == decode_to_unicode(keyvi_value)

    finally:
        shutil.rmtree(tmp_dir)

