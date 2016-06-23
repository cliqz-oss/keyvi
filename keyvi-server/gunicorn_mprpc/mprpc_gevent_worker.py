# -*- coding: utf-8 -*-

import errno
from gevent import socket

from gunicorn.workers.ggevent import GeventWorker
from mprpc import constants
import msgpack

class MPRPCGeventWorker(GeventWorker):
    def run(self):
        self.rpcserver_instance = self.app.mprpc()
        super(MPRPCGeventWorker, self).run()

    def handle(self, listener, client, addr):
        try:
            self.rpcserver_instance(client, addr)
        except socket.error as e:
            if e.args[0] not in (errno.EPIPE, errno.ECONNRESET):
                self.log.exception("Socket error processing request.")
            else:
                if e.args[0] == errno.ECONNRESET:
                    self.log.debug("Ignoring connection reset")
                else:
                    self.log.debug("Ignoring EPIPE")
        except Exception as e:
            self.handle_error(None, client, addr, e)
        finally:
            client.close()


        # custom error handler: we need to overwrite the default one which sends back HTTP
    def handle_error(self, req, client, addr, exc):

        self.log.exception("Error handling request %s", req)

        msg = (constants.MSGPACKRPC_RESPONSE, -1, exc.message, None)
        try:
            client.send(msgpack.dumps(msg))
        except:
            self.log.debug("Failed to send error message.")
