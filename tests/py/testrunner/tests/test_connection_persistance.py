import pytest
import requests
def test_connection_implicit_keepalive(exband_invoker, req):
    resp = req.get(exband_invoker.get_address())
    assert(resp.text.strip() == 'Not found')
    assert(resp.status_code == 404)
