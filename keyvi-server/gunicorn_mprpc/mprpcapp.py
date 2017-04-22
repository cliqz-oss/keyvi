# -*- coding: utf-8 -

from gunicorn.app.base import Application
from gunicorn import util

class MPRPCApplication(Application):
    def init(self, parser, opts, args):
        if len(args) != 1:
            parser.error("No application name specified.")
        self.cfg.set("default_proc_name", args[0])

        self.app_uri = args[0]

        # hardcode worker no matter what is set
        opts.worker_class = 'mprpc_gevent_worker.MPRPCGeventWorker'

    def load_mprpc_app(self):
        # load the app
        return util.import_app(self.app_uri)

    def load(self):
        self.mprpc = self.load_mprpc_app()


def run():
    from mprpcapp import MPRPCApplication
    MPRPCApplication("%(prog)s [OPTIONS] [APP_MODULE]").run()


if __name__ == '__main__':
    run()
