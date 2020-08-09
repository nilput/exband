import pytest
import requests
from .. import ConfigBuilder
import hashlib
from os.path import dirname, join, abspath

@pytest.fixture(scope='module')
def exband_fileserver(exband_factory_sm):
    rules = [
        {
            "prefix" : "/",
            "destination": {
                "type": "filesystem",
                "path": join(abspath(dirname(__file__)), 'data/public_html/'),
                "alias": False
            }
        },
        {
            "prefix" : "/things/",
            "destination": {
                "type": "filesystem",
                "path": join(abspath(dirname(__file__)), 'data/public_html/documents'),
                "alias": True
            }
        }
    ]
    yield exband_factory_sm(ConfigBuilder().default_http_server(rules=rules, port=8909).build())
    
def test_file_system_text_file(exband_fileserver, req):
    resp = req.get(exband_fileserver.get_address() + '/hello.txt')
    print(resp.text)
    assert(resp.text == 'hello world!')
    assert(resp.status_code == 200)

def test_file_system_known_files(exband_fileserver, req):
    # (url, md5, text)
    urls = [('/hello.txt', 'fc3ff98e8c6a0d3087d515c0473f8677', 'hello world!'),
            ('/files/primes.json', '00087cd6c64333d45f328c20dc68a7cb', None),
            ('/index.html', '5709aff9b1bc5663fd35835716b4df38', None),
            ('/hello.png', 'd9b24549189e135788fd45ca42bd7783', None)]
    for url, md5, text in urls:
        resp = req.get(exband_fileserver.get_address() + url, stream=True)
        if text:
            assert(resp.text == 'hello world!')
        response_md5 = hashlib.md5(resp.content).hexdigest()
        assert((url, response_md5) == (url, md5))
        assert(resp.status_code == 200)

def test_file_system_non_existent(exband_fileserver, req):
    bad = [
        '/hello.tx',
        '/hello.txtx',
        '/things/doesnt_exist.pdf',
        '/doesnt_exist',
        '/files/',
        '/files',
        '/'
    ]
    for url in bad:
        resp = req.get(exband_fileserver.get_address() + url)
        assert(resp.status_code == 404)
    
def test_file_system_pdf_aliased(exband_fileserver, req):
    resp = req.get(exband_fileserver.get_address() + '/things/dummy.pdf/')
    assert(resp.status_code == 404)
    resp = req.get(exband_fileserver.get_address() + '/documents/dummy.pdf')
    assert(resp.status_code == 404)
    resp = req.get(exband_fileserver.get_address() + '/things/dummy.pdf')
    assert(resp.status_code == 200)
    print(resp.raw)
    