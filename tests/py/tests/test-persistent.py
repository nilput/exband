import requests
import logging
logging.getLogger("requests").setLevel(logging.DEBUG)

rq = requests.Session()
resp = rq.get('http://127.0.0.1:8085/foo')
assert('/foo' in resp.text)
assert('/bar' not in resp.text)
print(resp)
resp = rq.get('http://127.0.0.1:8085/bar', timeout=2)
assert('/bar' in resp.text)
assert('/foo' not in resp.text)
print(resp)
