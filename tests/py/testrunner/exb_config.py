
class ConfigBuilder:
    def __init__(self):
        self.multiprocessing = False
        self.nprocesses = 0
        self.event_loops = 4
        self.polling = 'epoll'
        self.aio = False
        self.threadpool = False
        self.servers = []
        self.address = '127.0.0.1'
        self.port    = '8080'
    def default_http_server(self, rules=None, port=None):
        if port is None:
            port = self.port
        else:
            port = str(int(port))
        return self.server(port=port, rules=rules)
    def server(self, port=None, rules=None, domain=None, ssl=None):
        server = {}
        if ssl is not None:
            assert isinstance(ssl, dict)
        if port:
            server['listen'] = port
        if domain:
            server['server_name'] = domain
        if rules:
            assert isinstance(rules, (list, tuple))
            server['rules'] = rules
        self.servers.append(server)
        return self
    def build(self):
        conf = {}
        conf['event'] = {}
        conf['event']['processes'] = self.nprocesses
        conf['event']['loops'] = self.event_loops
        conf['event']['polling'] = self.polling
        conf['event']['aio'] = self.aio
        conf['event']['threadpool'] = self.threadpool
        conf['http'] = {}
        conf['http']['servers'] = []
        if self.servers:
            conf['http']['servers'].extend(self.servers)
        return conf
            
    