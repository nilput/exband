import os
import pytest
from .. import invoker
from .. import build_config as exb_build_config
from .. import ConfigBuilder
from ..http_requests import RequestsSession


pytest_config = None

def pytest_configure(config):
    global pytest_config
    pytest_config = config

def pytest_addoption(parser):
    parser.addoption(
        "--exband-no-invoke", action="store_true",help="my option: type1 or type2"
    )

def spawn_exband(config):
    if pytest_config.getoption("--exband-no-invoke", None):
        return invoker.ExbandAttachInvoker()
    else:
        return invoker.ExbandInvoker(exb_build_config['executable_path'], config)

@pytest.fixture()
def exband_invoker():
    server_config = ConfigBuilder().default_http_server().build()
    print(f'----\n${server_config}----\n')
    exb = None
    with spawn_exband(server_config) as exb:
        yield exb

def exband_factory_common():
    instances = []
    def factory(config):
        exb = spawn_exband(config)
        instances.append(exb)
        exb.run()
        print('started exband')
        exb.print()
        return exb
    yield factory
    for i in instances:
        i.close()
        print('closed exband')

@pytest.fixture()
def exband_factory():
    yield from exband_factory_common()

@pytest.fixture(scope='module')
def exband_factory_sm():
    yield from exband_factory_common()

@pytest.fixture()
def build_config():
    return exb_build_config

@pytest.fixture()
def req(build_config):
    if build_config['ssl_ca_file']:
        os.environ['REQUESTS_CA_BUNDLE'] = build_config['ssl_ca_file']
    session = RequestsSession(request_timeout=1.0)
    yield session
    session.close()