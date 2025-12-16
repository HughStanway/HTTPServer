from conftest import HttpServerRunner
from common import _make_request


def test_https_get_root_returns_ok(runnable_server_instance: HttpServerRunner):
    """
    Verifies that when server is running with HTTPS enabled it will accespt requests
    over HTTPS
    """
    # GIVEN:
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()
    
    # WHEN:
    response, body, tls = _make_request(
        "GET",
        "/",
        port=8443,
        use_https=True,
    )
        
    # THEN:
    assert tls is not None
    assert tls["version"].startswith("TLS")
    assert tls["cipher"] is not None
    assert "connected via secure TLS" in runnable_server_instance.get_output()
    assert response.status == 200
    assert 'OK' in body
    