from urllib.parse import urlparse
import socket

import requests

class RequestsSession(requests.Session):
    def __init__(self, *args, request_timeout=None, **kwargs):
        super().__init__(*args, **kwargs)
        self.request_timeout = request_timeout
    def get(self, *args, **kwargs):
        self._fill_kwargs(kwargs)
        return super().get(*args, **kwargs)
    def post(self, *args, **kwargs):
        self._fill_kwargs(kwargs)
        return super().post(*args, **kwargs)
    def head(self, *args, **kwargs):
        self._fill_kwargs(kwargs)
        return super().head(*args, **kwargs)
    def put(self, *args, **kwargs):
        self._fill_kwargs(kwargs)
        return super().put(*args, **kwargs)
    def delete(self, *args, **kwargs):
        self._fill_kwargs(kwargs)
        return super().delete(*args, **kwargs)
    def _fill_kwargs(self, kwargs):
        if ('timeout' not in kwargs) and (self.request_timeout):
            kwargs['timeout'] = self.request_timeout

class RawRequester:
    def __init__(self):
        pass
    def get(self, url, headers=None, http_version=None):
        if http_version is None:
            http_version = '1.1'
        if headers is None:
            headers = {}
        parsed = urlparse(url)
        if parsed.scheme != 'http':
            raise ValueError('expected an http scheme')
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # now connect to the web server on port 80 - the normal http port
        port = 80
        if parsed.port:
            port = int(parsed.port)
        sock.connect((parsed.hostname, port))
        
        
        