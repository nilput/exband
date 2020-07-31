import pytest
import requests
def test_default_request_handler(exband_invoker, req):
    print(exband_invoker)
    resp = req.get(exband_invoker.get_address())
    print(resp)
    assert(resp.text.strip() == 'Not found')
    assert(resp.status_code == 404)
