# -*- coding: utf-8 -*-

import json
import logging
import os
import pykeyvi
from mprpc import RPCServer

class IndexReader(RPCServer):
    def __init__(self, index_dir="kv-index"):
        self.index_dir = index_dir
        self.index_file = os.path.join(index_dir, "index.toc")
        self.log = logging.getLogger("kv-reader")
        self.log.info('Server started')
        self.toc = {}
        self.loaded_dicts = []
        self.compiler = None
        self.last_stat_rs_mtime = 0
        super(IndexReader, self).__init__(pack_params={'use_bin_type': True}, tcp_no_delay=True)


    def _load_index_file(self):
        toc = '\n'.join(open(self.index_file).readlines())
        try:
            new_toc = json.loads(toc)
            self.toc = new_toc
            self.log.info("loaded toc")
        except Exception, e:
            self.log.exception("failed to load toc")

    def _check_toc(self):
        try:
            stat_rs = os.stat(self.index_file)
        except:
            self.log.exception("could not load toc")
            return

        if stat_rs.st_mtime != self.last_stat_rs_mtime:
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
        if type(key) == unicode:
            key = key.encode("utf-8")

        self._check_toc()

        for d in self.loaded_dicts:
            m = d.get(key)
            if m is not None:
                return m.dumps()
        return None
