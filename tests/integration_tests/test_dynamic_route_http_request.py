from conftest import HttpServerRunner
from common import _make_request


def test_server_matches_dynamic_route(runnable_server_instance: HttpServerRunner):
    """
    Verify dynamic routes can be matched with mutiple route paths
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    
    p1 = "foo"
    p2 = "bar"
     
    # WHEN:
    r1, b1 = _make_request("GET", f"/dynamic/{p1}")
    r2, b2 = _make_request("GET", f"/dynamic/{p2}")
    
    # THEN:
    assert r1.status == 200
    assert f"GET [dynamic] request recieved: {p1}" in b1
    
    assert r2.status == 200
    assert f"GET [dynamic] request recieved: {p2}" in b2