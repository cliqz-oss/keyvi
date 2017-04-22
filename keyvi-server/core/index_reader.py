# -*- coding: utf-8 -*-

import json
import logging
import os
import pykeyvi
import gevent
import signal
from gevent.server import StreamServer
from mprpc import RPCServer



def start_reader(ip, port, idx, interval):
    """
    Starts a simple reader, rather for testing purposes.
    :param ip: ip to listen to
    :param port:  port
    :param idx: indexing directory
    """
    # disable CTRL-C for the worker, handle it only in the parent process
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    index_reader = IndexReader(idx, interval)

    server = StreamServer((ip, port), index_reader)
    server.serve_forever()


class IndexReader(RPCServer):
    class IndexRefresh(gevent.Greenlet):
        def __init__(self, reader, interval=1):
            self._reader = reader
            self._delay = interval
            super(IndexReader.IndexRefresh, self).__init__()
        def _run(self):
            sleep_time = self._delay
            while True:
                gevent.sleep(sleep_time)
                try:
                    self._reader.check_toc()
                except:
                    self._reader.log.exception('Failed to refresh index')

    def __init__(self, index_dir="kv-index", refresh_interval=1):
        self.index_dir = index_dir
        self.index_file = os.path.join(index_dir, "index.toc")
        self.log = logging.getLogger("kv-reader")
        self.log.info('Server started, Index: {}, Refresh: {}'.format(self.index_dir, refresh_interval))
        self.toc = {}
        self.loaded_dicts = []
        self.compiler = None
        self.last_stat_rs_mtime = 0

        self._refresh = IndexReader.IndexRefresh(self, refresh_interval)
        self._refresh.start()

        super(IndexReader, self).__init__(pack_params={'use_bin_type': True}, tcp_no_delay=True)


    def _load_index_file(self):
        toc = '\n'.join(open(self.index_file).readlines())
        try:
            new_toc = json.loads(toc)
            self.toc = new_toc
            self.log.info("loaded toc")
        except Exception as e:
            self.log.exception("failed to load toc")

    def check_toc(self, force=False):
        try:
            stat_rs = os.stat(self.index_file)
        except:
            self.log.exception("could not load toc")
            return

        if stat_rs.st_mtime != self.last_stat_rs_mtime or force:
            self.log.info("reload toc")
            self.last_stat_rs_mtime = stat_rs.st_mtime
            self._load_index_file()
            new_list_of_dicts = []
            self.log.info("loading files")
            files = self.toc.get('files', [])
            files.reverse()
            for f in files:
                filename = f.encode("utf-8")
                self.log.info("Loading: @@@{}@@@".format(filename))
                new_list_of_dicts.append(pykeyvi.Dictionary(filename))
                self.log.info("load dictionary {}".format(filename))
            self.loaded_dicts = new_list_of_dicts

    def get(self, key):
        if key is None:
            return None

        for d in self.loaded_dicts:
            m = d.get(key)
            if m is not None:
                return m.dumps()
        return None

    def exists(self, key):
        if key is None:
            return False

        for d in self.loaded_dicts:
            if key in d:
                return True
        return False

    def reload(self):
        self.check_toc(True)
