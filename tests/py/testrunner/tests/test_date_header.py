from datetime import datetime, timezone, timedelta
import pytest
import requests
from dateutil.parser import parse


def test_connection_implicit_keepalive(exband_invoker, req):
    resp = req.get(exband_invoker.get_address())
    print(resp.headers)
    assert('Date' in resp.headers)
    date = parse(resp.headers['Date'])
    print(date)
    diff = datetime.now(timezone.utc) - date
    assert(diff < timedelta(seconds=1))
    
    
