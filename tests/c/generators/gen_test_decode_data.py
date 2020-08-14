from cutils import c_literal
import random
import urllib.parse

random.seed(232)
raw = []
quoted = []
for n in range(50):
    slen = random.randint(0, 60)
    s = b''
    while len(s) < slen:
        b = random.randint(0, 0xFF)
        if n < 10:
            try:
                if not str.isprintable(chr(b)):
                    continue
            except:
                continue
        s += bytes([b])
    raw.append(s)
    quoted += [urllib.parse.quote_from_bytes(s)]

eparts = []
keys = ['key1', 'key2', 'repeated_key', 'repeated_key', 'key with spaces']
for i in range(14):
    d = []
    for j in range(i):
        try:
            k = keys[j].encode('utf-8')
        except:
            k = 'key{}'.format(j).encode('utf-8')
        v = raw[j]
        d += [(k,v)]
    eparts.append(d)

f = open('test_decode_data.h', 'w')
print('struct encdec ed[] = {', file=f)
for q in quoted:
    enc = q.encode('utf-8')
    dec = urllib.parse.unquote_to_bytes(q.replace('+', '%20'))
    print('{{.enc=(unsigned char []){{ {} }}, .enclen={},\n .dec=(unsigned char []){{ {} }}, .declen={}}},'.format(c_literal(enc), len(enc), c_literal(dec), len(dec)), file=f)
print('};', file=f)
print ('struct encdec_keys_values eparts[] = {', file=f)
for d in eparts:
    enc = urllib.parse.urlencode(d).encode('utf-8')
    keys_values = '{'
    for k,v in d:
        keys_values += '\n      {{ .key=(unsigned char []){{ {} }}, \n      .value = (unsigned char []){{ {} }}, \n      .keylen = {}, .valuelen = {} \n      }},'.format(c_literal(k), c_literal(v), len(k), len(v))
    keys_values += '}'
    encdec_keys_values = '  {{ \n    .enc = (unsigned char []){{ {} }}, \n    .enclen = {}, \n    .keyvalues = (struct keyval[]){}, \n    .nvalues = {} }},'.format(c_literal(enc), len(enc), keys_values, len(d))
    print(encdec_keys_values, file=f)
print('};', file=f)
f.close()
