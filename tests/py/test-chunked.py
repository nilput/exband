import http.client

def chunk_data(data, chunk_size):
    dl = len(data)
    ret = ""
    for i in range(dl // chunk_size):
        clen = hex(chunk_size)[2:]
        chunk = data[i * chunk_size : (i + 1) * chunk_size]
        ret += "%s\r\n" % (clen)
        ret += "%s\r\n" % (chunk)

    if len(data) % chunk_size != 0:
        ret += "%s\r\n" % (hex(len(data) % chunk_size)[2:])
        ret += "%s\r\n" % (data[-(len(data) % chunk_size):])

    ret += "0\r\n\r\n"
    return ret
host = '127.0.0.1:8085'
conn = http.client.HTTPConnection(host)
url = "/post/"
conn.putrequest('POST', url)
conn.putheader('Transfer-Encoding', 'chunked')
conn.endheaders()
body = 'hello world! ' * 7
size_per_chunk = len(body) // 6

conn.send(chunk_data(body, size_per_chunk).encode('utf-8'))

resp = conn.getresponse()
print(resp.status, resp.reason)
print(resp.read())
