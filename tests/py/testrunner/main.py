import pytest
import os
import time
from . import invoker
from .utils import *
import requests

dir_path = os.path.dirname(os.path.realpath(__file__))
EXBAND_PROJECT_DIR = os.path.join(dir_path, '../../../')
EXBAND_EXECUTABLE_PATH = os.path.join(EXBAND_PROJECT_DIR, exe_name('exb'))
EXBAND_SSL_CA_CERT_FILE = None
EXBAND_HAS_SSL = True

if os.path.exists(os.path.join(EXBAND_PROJECT_DIR, 'tests/ssl_certs/ssl_dir/ca.crt')):
    EXBAND_SSL_CA_CERT_FILE = os.path.join(EXBAND_PROJECT_DIR, 'tests/ssl_certs/ssl_dir/ca.crt')


build_config = {
    'executable_path':  EXBAND_EXECUTABLE_PATH,
    'has_ssl':          EXBAND_HAS_SSL,
    'ssl_ca_file':      EXBAND_SSL_CA_CERT_FILE,
    'project_dir':      EXBAND_PROJECT_DIR
}

def main():
    exb = invoker.ExbandInvoker(EXBAND_EXECUTABLE_PATH)
    with exb:
        url = 'http://127.0.0.1:8080/'
        print('waiting 1 second for server to start..')
        time.sleep(1)
        resp = requests.get(url)
        print(f'--- Made request to ${url}:     Status: ${resp.status_code} ---')
        print(f'--- Headers [{len(resp.headers)} headers]---')
        print(format_headers(resp.headers))
        print(f'--- End of Headers ---')
        print(f'--- Body Text [{len(resp.text)} bytes]---')
        print(resp.text)
        print(f'--- End of body ---')
        

if __name__ == '__main__':
    main()