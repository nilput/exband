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