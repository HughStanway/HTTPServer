from http.client import HTTPConnection
from conftest import HttpServerRunner
from common import _make_request


def test_get_root_returns_ok(runnable_server_instance: HttpServerRunner):
    """
    Simple integration test: GET / should return 200 and the expected body.
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN:
    response, body = _make_request("GET", "/")

    # THEN:
    assert response.status == 200
    assert 'OK' in body


def test_get_nonexistent_route_returns_404(runnable_server_instance: HttpServerRunner):
    """
    Verifies that the server router sends a 404 response when a non-existient route
    is requested
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN:
    response, body = _make_request("GET", "/nonexistient")

    # THEN:
    assert response.status == 404
    assert '404 Not Found: /nonexistient' in body


def test_keepalive_allows_multiple_requests_on_same_connection(runnable_server_instance: HttpServerRunner):
    """
    Verify that a single connection can be reused for multiple requests (keep-alive) and is kept
    open by the server
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN and THEN:
    conn = HTTPConnection('127.0.0.1', 8080, timeout=2)

    # First request
    conn.request('GET', '/')
    r1 = conn.getresponse()
    b1 = r1.read().decode('utf-8')
    assert r1.status == 200
    assert 'OK' in b1

    # Second request
    conn.request('GET', '/')
    r2 = conn.getresponse()
    b2 = r2.read().decode('utf-8')
    assert r2.status == 200
    assert 'OK' in b2

    conn.close()


def test_connection_close_header_closes_connection(runnable_server_instance: HttpServerRunner):
    """
    If the client sends 'Connection: close', the server should include 'Connection: close'
    and close the socket after responding.
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN:
    response, body = _make_request('GET', '/', {'Connection': 'close'})

    # THEN:
    assert response.status == 200
    assert response.getheader('Connection') in ('close', 'Close', 'CLOSE')
    assert 'OK' in body
