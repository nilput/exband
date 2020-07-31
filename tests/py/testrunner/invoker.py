import subprocess
import tempfile
import json
import time
from pathlib import Path

from . import exb_config

def default_config():
    return exb_config.ConfigBuilder().default_http_server().build()
def config_to_string(config):
    if isinstance(config, str):
        return config
    elif isinstance(config, dict):
        return json.dumps(config)
    raise ValueError('invalid config type, expected a string or a dictionary')

#doesn't actually start the server, assumes it's already runnning
class ExbandRunningInvoker:
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

    #returns something like: http://127.0.0.1:8080
    def get_address(self, domain=None, ssl=False):
        return 'http://127.0.0.1:8080'

    def run(self):
        self.process = subprocess.Popen([self.executable_path, *self.exband_args])
        time.sleep(self.start_warmup_time)

    def close(self):
        self.process.terminate()
        self.process.wait(self.terminate_timeout)
        return self.process.returncode

    def __enter__(self):
        self.run()
        return self
    def __exit__(self, type, value, traceback):
        #Exception handling here
        self.close()
