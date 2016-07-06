# -*- coding: utf-8 -*-

import random
import contextlib
import tempfile
import multiprocessing
import os
import shutil
from mprpc import RPCClient

import sys
root = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(root, "../"))

from core import index_writer, index_reader


class TestServer():
    def __init__(self, tmp_directory):
        randport = random.randint(0, 1999)
        self.readerp = 6000 + randport
        self.writerp = 6000 + randport + 1

        self.reader = multiprocessing.Process(target=index_reader.start_reader,
                                    args=('127.0.0.1', self.readerp, tmp_directory, 0.1))

        self.writer = multiprocessing.Process(target=index_writer.start_writer,
                                    args=('127.0.0.1', self.writerp, tmp_directory),
                                    kwargs={"segment_write_interval": 0.1})
        self.reader.start()
        self.writer.start()

    def __del__(self):
        self.reader.terminate()
        self.writer.terminate()

    def get_reader(self):
        return RPCClient('127.0.0.1', self.readerp)

    def get_writer(self):
        return RPCClient('127.0.0.1', self.writerp)

    def sync(self):
        self.get_writer().call('commit', False)
        self.get_reader().call('reload')


@contextlib.contextmanager
def tmp_server():
    tmp_dir = tempfile.mkdtemp()
    print(tmp_dir)
    t = TestServer(tmp_dir)
    yield t
    del t

    #shutil.rmtree(tmp_dir, ignore_errors=True)