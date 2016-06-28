from __future__ import unicode_literals

import sys
import multiprocessing
import logging
import gunicorn.app.base
import gunicorn.util

from gunicorn.six import iteritems
import gevent
from gevent.server import StreamServer

from cfg import keyvi_server_conf as conf
import core.index_reader
import core.index_writer
import core.kvs_logging

class StandaloneApplication(gunicorn.app.base.BaseApplication):

    def __init__(self, options=None):
        self.options = options or {}

        super(StandaloneApplication, self).__init__()

    def load_config(self):
        config = dict([(key, value) for key, value in iteritems(self.options)
                       if key in self.cfg.settings and value is not None])
        for key, value in iteritems(config):
            self.cfg.set(key.lower(), value)

    def load(self):
        self.mprpc = core.index_reader.IndexReader


def start_reader():

    options = {
        'bind': '%s:%s' % (conf.reader_ip, conf.reader_port),
        'workers': conf.reader_workers,
        'worker_class': 'gunicorn_mprpc.mprpc_gevent_worker.MPRPCGeventWorker'

    }

    StandaloneApplication(options).run()

def create_writer(conf):
    return multiprocessing.Process(target=core.index_writer.start_writer,
                                    args=(conf.writer_ip, conf.writer_port, index_dir,
                                    conf.merge_processes * 2, conf.segment_write_trigger, conf.segment_write_interval))

if __name__ == '__main__':
    index_dir = conf.index_dir
    merge_processes = conf.merge_processes

    core.kvs_logging.setup_logging()
    logger = logging.getLogger('kv-server')

    reader = multiprocessing.Process(target=start_reader)
    reader.start()

    merge_workers = {}

    for idx in range(0, merge_processes):
        logger.info("Start merge worker {}".format(idx))

        worker = multiprocessing.Process(target=core.index_writer.start_merge_worker,
                                    args=(idx, index_dir))
        worker.start()
        merge_workers[idx] = worker

    writer = create_writer(conf)
    writer.start()

    try:
        while True:
            gevent.sleep(1.0)
            for idx, worker in merge_workers.iteritems():
                if not worker.is_alive():

                    worker.join()
                    logging.warning("Merge worker died ({}), restarting Merger".format(worker.exitcode))
                    new_worker = multiprocessing.Process(target=core.index_writer.start_merge_worker,
                                        args=(idx, index_dir, conf.writer_port))
                    new_worker.start()
                    merge_workers[idx] = new_worker
            if not reader.is_alive():
                reader.join()
                logger.warning("Reader master died ({}), restarting Reader".format(reader.exitcode))
                reader = multiprocessing.Process(target=start_reader)
                reader.start()
            if not writer.is_alive():
                writer.join()
                logger.warning("Writer died ({}), restarting Writer".format(reader.exitcode))
                writer = create_writer(conf)
                writer.start()

    except KeyboardInterrupt:
        logger.info('Received ctrl-c, exiting')
        for idx, worker in merge_workers.iteritems():
            logging.info("Shutdown merger worker {}".format(idx))
            worker.terminate()
            worker.join()
        logging.info("Shutdown writer")
        writer.terminate()
        writer.join()
        logging.info("Shutdown reader")
        reader.terminate()
        reader.join()
        sys.exit(0)

