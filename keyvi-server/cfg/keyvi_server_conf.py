# coding: utf-8

# directory where the data is stored
index_dir = "kv-index"

# number of (background)  merge processes
merge_processes = 2

# ip/port where writer listens to
writer_port = 6101
writer_ip = '127.0.0.1'

# trigger to write a segment after x insert even if write interval has not been reached yet
segment_write_trigger = 10000

# write interval in seconds
segment_write_interval = 1

# ip/port where readers listens to
reader_port = 6100
reader_ip = '0.0.0.0'
reader_workers = 10

# interval to re-read the toc
reader_refresh = 1
