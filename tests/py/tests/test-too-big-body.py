import requests
import logging
logging.getLogger("requests").setLevel(logging.DEBUG)

rq = requests.Session()
few_bytes = b'\xfe\xed\xbe\xef' * 4
data = few_bytes
resp = rq.post('http://127.0.0.1:8085/post/', data=data)
print(resp)

kilobyte = b'\xfe\xed\xbe\xef' * 1024 
data = 1 * kilobyte
resp = rq.post('http://127.0.0.1:8085/post/', data=data)
print(resp)

megabyte = kilobyte * 1024
data = 10 * megabyte
resp = rq.post('http://127.0.0.1:8085/post/', data=data)
print(resp)
