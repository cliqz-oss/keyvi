## Experimental kevyi server implementation

# YOU HAVE BEEN WARNED: THIS IS NOT PRODUCTION READY

To run it, start the server with:

    python keyvi-server.py

Use mprpc to communicate with the server:

writer:

    from mprpc import RPCClient
    cw=RPCClient('localhost', 6101)
    cw.call('set', 'dilbert', "{'what':'developer'}")
    cw.call('set', 'wally', "{'what':'developer'}")
    cw.call('set', 'phb', "{'what':'manager'}")

reader:

    import pykeyvi
    from mprpc import RPCClient
    cr=RPCClient('localhost', 6100)
    x=cr.call('get', 'dilbert')
    pykeyvi.Match.loads(x).GetValue()


### How it works

Data is collected and flushed after a certain interval. 
The merger picks up segments and merges them to compact the index and reduce the number of segments.
The index.toc file is used to keep a list of segments that define the current index. That file is checked/read from the worker processes.

Watch the kv-index subfolder to get an idea.
