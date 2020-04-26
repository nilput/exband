http_port = 8085
polling_backend = epoll
http_aio = 0
threadpool_size = 0
n_event_loops = 4
http_server_module = examples/basic/basic.so:handler
http_server_module_args = db_path=./db.sqlite3 db_reinit=0 site_link=http://127.0.0.1:8085
