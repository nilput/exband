import pytest
from .. import invoker
from ..main import EXBAND_EXECUTABLE_PATH
from ..exb_config import ConfigBuilder
from ..http_requests import RequestsSession


pytest_config = None

def pytest_configure(config):
    global pytest_config
    pytest_config = config

def pytest_addoption(parser):
    parser.addoption(
        "--exband-no-invoke", action="store_true",help="my option: type1 or type2"
    )

@pytest.fixture()
def exband_invoker(server_config=None):
    if server_config is None:
        server_config = ConfigBuilder().default_http_server().build()
        print(f'----\n${server_config}----\n')
    exb = None
    if pytest_config.getoption("--exband-no-invoke", None):
        exb = invoker.ExbandRunningInvoker()
    else:
        exb = invoker.ExbandInvoker(EXBAND_EXECUTABLE_PATH, server_config)
    with exb:
        yield exb

@pytest.fixture()
def req():
    session = RequestsSession(request_timeout=1.0)
    yield session
    session.close()