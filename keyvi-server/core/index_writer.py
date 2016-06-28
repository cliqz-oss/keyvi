# -*- coding: utf-8 -*-

import os
import signal
import logging
import pykeyvi
from datetime import datetime
import time
import multiprocessing
import gevent
import gevent.lock
import json
from shutil import move
from mprpc import RPCServer
from mprpc import RPCClient
from gevent.server import StreamServer

LOG = logging.getLogger('keyvi-writer')

# Queue we put the compilation tasks into
MERGER_QUEUE = multiprocessing.JoinableQueue()

def start_writer(ip, port, idx):
    # disable CTRL-C for the worker, handle it only in the parent process
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    index_writer = IndexWriter(idx)

    server = StreamServer((ip, port), index_writer)
    server.serve_forever()

def start_merge_worker(idx, index_dir="kv-index", writer_port=6101):
    # disable CTRL-C for the worker, handle it only in the parent process
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    LOG.info("Merge Worker {} started".format(idx))

    while True:
        job = MERGER_QUEUE.get()
        try:
            new_segment = _merge(job, index_dir)
        except:
            LOG.exception("Merge failed, worker {}".format(idx))
            raise

        LOG.info("call finalize_merge, worker {}".format(idx))
        try:
            c = RPCClient('localhost', writer_port)
            c.call('finalize_merge', job, new_segment)
        except:
            LOG.exception('failed to call finalize')

        LOG.info("ready for next merge, worker {}".format(idx))

def _merge(list_to_merge, index_dir):

    LOG.info("start merge")
    merger = pykeyvi.JsonDictionaryMerger()
    for f in list_to_merge:
        if type(f) == unicode:
            f = f.encode("utf-8")

        LOG.info('add to merger: {}'.format(f))
        merger.Add(f)

    filename = _get_segment_name(index_dir)

    merger.Merge(filename)
    LOG.info("finished merge")

    return filename

def _get_segment_name(index_dir, prefix='master'):
    filename = os.path.join(index_dir, "{}-{}-{}.kv".format(prefix, int(time.time() * 1000000), os.getpid()))
    if type(filename) == unicode:
        filename = filename.encode("utf-8")
    return filename

class IndexWriter(RPCServer):

    class IndexFinalizer(gevent.Greenlet):
        def __init__(self, writer, interval=1):
            self._writer = writer
            self._delay = interval
            self.last_commit = 0
            self.sem_compiler = gevent.lock.BoundedSemaphore(5)
            super(IndexWriter.IndexFinalizer, self).__init__()

        def _run(self):
            sleep_time = self._delay
            while True:
                gevent.sleep(sleep_time)
                sleep_time = self._commit_checked()
                self._writer.find_merges()

        def _commit_checked(self):
            now = time.time()
            if now - self.last_commit >= self._delay:
                self.commit()
                return self._delay
            else:
                return now - self.last_commit


        def commit(self, async=True):
            if self._writer.compiler is None:
                return

            compiler = self._writer.compiler
            self._writer.compiler = None

            # check it again, to be sure
            if compiler is None:
                return
            LOG.info("creating segment")

            def run_compile(compiler):
                if self.sem_compiler.locked():
                    LOG.warning("To many open compilers")

                with self.sem_compiler:
                    compiler.Compile()
                    filename = _get_segment_name(self._writer.index_dir)
                    compiler.WriteToFile(filename + ".part")
                    os.rename(filename+".part", filename)

                    self._writer.register_new_segment(filename)

            self.last_commit = time.time()
            if async:
                gevent.spawn(run_compile, compiler)
            else:
                run_compile(compiler)

    def __init__(self, index_dir="kv-index", merge_queue=4, segment_write_trigger=10000, segment_write_interval=10):
        self.index_dir = index_dir
        self.log = LOG
        self.index_file = os.path.join(index_dir, "index.toc")
        self.log.info('Writer started')
        self.segments_in_merger = {}
        self.merger_lock = gevent.lock.RLock()
        self.segments = []
        self.segments_marked_for_merge = []
        self._load_index_file()
        self.write_counter = 0

        # blocking queue leads to a deadlock, therefore checking ourselves
        self.merger_queue_max_size = merge_queue

        self.segment_write_trigger = segment_write_trigger

        self._finalizer = IndexWriter.IndexFinalizer(self, segment_write_interval)
        self._finalizer.start()

        self.compiler = None
        super(IndexWriter, self).__init__(pack_params={'use_bin_type': True}, tcp_no_delay=True)

    def _load_index_file(self):

        if not os.path.exists(self.index_file):
            return
        toc = '\n'.join(open(self.index_file).readlines())
        try:
            toc = json.loads(toc)
            self.segments = toc.get('files', [])
            self.log.info("loaded index")

        except Exception, e:
            self.log.exception("failed to load index")
            raise

    def _init_lazy_compiler(self):
        if not self.compiler:
            self.compiler = pykeyvi.JsonDictionaryCompiler(1024*1024*10, {"stable_insert": "true"})
            self.write_counter = 0

    def find_merges(self):
        if (len(self.segments) - len(self.segments_marked_for_merge)) < 2 or MERGER_QUEUE.qsize() >= self.merger_queue_max_size:
            #LOG.info("skip merge, to many items in queue or to few segments")
            return []

        to_merge = []
        with self.merger_lock:

            for segment in list(self.segments):
                if segment not in self.segments_marked_for_merge:
                    self.log.info("add to merge list {}".format(segment))
                    to_merge.append(segment)

            if len(to_merge) > 1:
                self.segments_marked_for_merge.extend(to_merge)
                to_merge.reverse()
                self.log.info("Start merge of {} segments".format(len(to_merge)))
                MERGER_QUEUE.put(to_merge)

    def _write_toc(self):
        try:
            with self.merger_lock:
                self.log.info("write new TOC")
                toc = json.dumps({"files": self.segments})
                fd = open("index.toc.new", "w")
                fd.write(toc)
                fd.close()
                move("index.toc.new", self.index_file)
        except:
            self.log.exception("failed to write toc")
            raise

    def register_new_segment(self, new_segment):
        # add new segment
        with self.merger_lock:
            self.log.info("add {}".format(new_segment))

            self.segments.append(new_segment)

        # re-write toc to make new segment available
        self._write_toc()
        return

    def ping(self, x):
        return "HELLO " + x

    def finalize_merge(self, job, new_segment):
        self.log.info("finalize_merge called")
        try:
            self.log.info("finalize merge, put it into the index")
            new_segments = []
            merged = False
            with self.merger_lock:
                for s in self.segments:
                    if s in job:
                        if not merged:
                            # found the place where merged segment should go in
                            new_segments.append(new_segment)
                            merged = True
                    else:
                        new_segments.append(s)

                self.log.info("Segments: {}".format(new_segments))
                self.segments = new_segments

                # remove from marker list
                for item in job:
                    self.segments_marked_for_merge.remove(item)

            self._write_toc()

            # delete old files
            for f in job:
                os.remove(f)

        except:
            self.log.exception("Failed to finalize index")
            raise

        return "SUCCESS"


    def set(self, key, value):
        if key is None:
            return

        self._init_lazy_compiler()
        self.compiler.Add(key, value)
        self.write_counter += 1

        if self.write_counter >= self.segment_write_trigger:
            self._finalizer.commit()

        return

    def set_many(self, key_value_pairs):
        for key, value in key_value_pairs:
            if key is None:
                continue
            self._init_lazy_compiler()
            self.compiler.Add(key, value)
            self.write_counter += 1
        if self.write_counter >= self.segment_write_trigger:
            self._finalizer.commit()

    def set_many_bulk(self, client_token, key_value_pairs, optimistic=False):
        for key, value in key_value_pairs:
            if type(key) == unicode:
                key = key.encode("utf-8")
            if type(value) == unicode:
                value = value.encode("utf-8")

            # todo: use bulk compiler
            if self.compiler is None:
                self.compiler=self._init_lazy_compiler()
            self.compiler.Add(key, value)

    def commit(self, async=True):
        self._finalizer.commit()
