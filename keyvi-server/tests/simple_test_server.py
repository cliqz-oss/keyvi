# -*- coding: utf-8 -*-

import random
import contextlib
import tempfile
import multiprocessing
import os
import shutil
import time
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
        self.terminated = False


    def terminate(self):
        self.reader.terminate()
        self.writer.terminate()
        self.terminated = True

    def __del__(self):
        if not self.terminated:
            self.terminate()

    def get_reader(self):
        attempt = 1

        while attempt < 5:
            try:
                c = RPCClient('127.0.0.1', self.readerp)
                return c
            except:
                pass
            attempt += 1
            time.sleep(0.5)
        raise Exception("Could not instantiate reader")

    def get_writer(self):
        attempt = 1

        while attempt < 5:
            try:
                c = RPCClient('127.0.0.1', self.writerp)
                return c
            except:
                pass
            attempt += 1
            time.sleep(0.5)
        raise Exception("Could not instantiate writer")

    def sync(self):
        self.get_writer().call('commit', False)
        self.get_reader().call('reload')


@contextlib.contextmanager
def tmp_server():
    tmp_dir = tempfile.mkdtemp()
    try:
        t = TestServer(tmp_dir)
        yield t
    finally:
        t.terminate()
        del t
        shutil.rmtree(tmp_dir, ignore_errors=True)