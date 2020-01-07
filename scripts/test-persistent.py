import requests
import logging
logging.getLogger("requests").setLevel(logging.DEBUG)

rq = requests.Session()
resp = rq.get('http://127.0.0.1:8085/foo')
print(resp)
resp = rq.get('http://127.0.0.1:8085/bar', timeout=2)
print(resp)