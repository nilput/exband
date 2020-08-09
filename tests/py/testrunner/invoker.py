import subprocess
import tempfile
import json
import time
import sys
import re
from fnmatch import fnmatch
from pathlib import Path

from . import exb_config

def log(level, msg):
    assert level in ('e', 'i', 'w', 'd')
    print(msg)

def default_config():
    return exb_config.ConfigBuilder().default_http_server().build()
def config_to_string(config):
    if isinstance(config, str):
        return config
    elif isinstance(config, dict):
        return json.dumps(config)
    raise ValueError('invalid config type, expected a string or a dictionary')

#doesn't actually start the server, assumes it's already runnning
class ExbandAttachInvoker:
    def __init__(self, *args, **kwargs):
        pass
    #returns something like: http://127.0.0.1:8080
    def get_address(self, domain=None, ssl=False):
        return 'http://127.0.0.1:8080'

    def run(self):
        pass

    def close(self):
        pass

    def __enter__(self):
        return self
    def __exit__(self, type, value, traceback):
        pass

def address_port_pair(address):
    split = address.split(':', maxsplit=1)
    good_ip_regex = r'((((([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])))|(((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?))'
    regex = good_ip_regex + '(:[0-9]+)?|([0-9]+)'
    m = re.match(regex, address)
    if not m:
        raise ValueError('expected "{}" to be of the form ip:port'.format(address))
    ip = m.group(1)
    port = m.group(83) if m.group(83) else m.group(84)
    return (ip, port)

class ExbandInvoker:
    def __init__(self,
                 executable_path,
                 config=None,
                 extra_args=None,
                 tmp_dir_path = None,
                 terminate_timeout=5.0):
        if config is None:
            config = default_config()
        if tmp_dir_path is None:
            tmp_dir_path = Path(tempfile.mkdtemp(prefix='exbandtests-'))
        else:
            tmp_dir_path = Path(tmp_dir_path)
        if extra_args is None:
            extra_args = ()
        self.config = config_to_string(config)
        self.process = None
        self.terminate_timeout = terminate_timeout
        self.start_warmup_time = 1.0
        self.tmp_dir_path = tmp_dir_path
        self.tmp_config_path = tmp_dir_path / 'exband.json'
        self.tmp_config_path.write_text(self.config)
        self.exband_args = ['-c', self.tmp_config_path.absolute().as_posix(), *extra_args]
        self.executable_path = executable_path
    
    def _get_config_as_dict(self):
        return json.loads(self.config)

    #returns something like: http://127.0.0.1:8080
    def get_address(self, domain=None, ssl=False, index=None, ip=None, port=None):
        if not self.process:
            raise RuntimeError('illegal state, tried to get address before exband was run')
        config = self._get_config_as_dict()
        if not config.get('http') or not config['http'].get('servers') or len(config['http']['servers']) == 0:
            raise RuntimeError('exband config doesn\'t contain servers')
        servers = config['http']['servers']

        best_match = None
    
        for i, server in enumerate(servers):
            match_points = 0
            listen = None
            if ssl:
                if not server.get('ssl') or not server['ssl'].get('listen'):
                    continue
                listen = server['ssl']['listen']
            else:
                if not server.get('listen'):
                    continue
                listen = server['listen']
            server_ip, server_port = address_port_pair(listen)
            if index is not None and i == index:
                match_points += 10
            if ip is not None and server_ip != ip:
                continue
            if port is not None and server_port != port:
                continue
            if server.get('server_name') and domain and fnmatch(domain, server.get('server_name')):
                match_points += 1
            if (best_match is None) or match_points > best_match[1]:
                address = 'http' + ('s' if ssl else '') + '://' + \
                           (server_ip if server_ip else '127.0.0.1') +  \
                           ((':' + server_port) if server_port else '')
                best_match = (i, match_points, address)

        if index is not None:
            if index >= len(servers) or index < 0:
                raise RuntimeError('invalid config index specified,' +
                                ' specified index: {}, number of servers: {}'.format(index, len(servers)))
        if best_match is None:
            raise RuntimeError('no server matching the specified criteria was found')    
        return best_match[2]

    def run(self):
        self.process = subprocess.Popen([self.executable_path, *self.exband_args], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        addr = None
        try:
            addr = self.get_address()
        except:
            pass
        log('i', 'started exband, \ntmppath: {}\naddress: {}'.format(self.tmp_dir_path, addr))
        time.sleep(self.start_warmup_time)
        try:
            self.process.wait(0.1)
            self.print()
            raise subprocess.SubprocessError()
        except subprocess.TimeoutExpired as e:
            pass

    def close(self):
        if not self.process:
            log('e', 'found no process when closing exband')
            return
        self.process.terminate()
        self.process.wait(self.terminate_timeout)
        self.print()
        log('i', 'closed exband')
        return self.process.returncode

    def print(self):
        stderr = None
        stdout = None
        try:
            stderr, stdout = self.process.communicate(timeout=0)
        except subprocess.TimeoutExpired as e:
            stderr = e.stderr
            stdout = e.stdout
        for b in [stderr, stdout]:
            if b:
                print(b.decode('utf-8'), file=sys.stderr)
        

    def __enter__(self):
        self.run()
        return self
    def __exit__(self, type, value, traceback):
        #Exception handling here
        self.close()
