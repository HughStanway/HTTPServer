import pytest # type: ignore
import socket
from http.client import HTTPConnection, RemoteDisconnected
from conftest import HttpServerRunner
from common import _make_request


def _setup_config(
    runnable_server_instance: HttpServerRunner,
):
    runnable_server_instance.set_config_value("https.enable_http_redirection", True)
    runnable_server_instance.set_config_value("ports.http_redirection_port", 8081)
    
def test_redirection_server_only_starts_when_enabled(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that the redirection server only starts when both HTTPS and HTTP redirection
    are enabled in the configuration.
    """
    # GIVEN
    runnable_server_instance.set_config_value("https.enable_https", True)
    runnable_server_instance.set_config_value("https.enable_http_redirection", False)

    # WHEN
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()
    
    # THEN
    log_output = runnable_server_instance.get_output()
    assert "event=redirection_server_starting" not in log_output
    
def test_redirection_server_not_started_when_redirection_port_conflicts_with_main_port(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that the redirection server does not start if the redirection port is the same
    as the main server port, and that an appropriate warning is logged.
    """
    # GIVEN   
    runnable_server_instance.set_config_value("https.enable_https", True)
    runnable_server_instance.set_config_value("https.enable_http_redirection", True)
    runnable_server_instance.set_config_value("ports.http_redirection_port", 8080)

    # WHEN
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()

    # THEN
    log_output = runnable_server_instance.get_output()
    assert "event=redirection_port_conflict" in log_output

def test_redirection_server_redirects_to_https(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that the redirection server correctly redirects HTTP requests to HTTPS.
    """
    # GIVEN
    REDIRECTION_PORT = 8081
    
    _setup_config(runnable_server_instance)
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()

    # WHEN
    runnable_server_instance.wait_for_output("event=redirection_server_running")
    response, body = _make_request("GET", "/", {"Connection": "close"}, port=REDIRECTION_PORT)

    # THEN
    assert response.status == 301
    assert "Location" in response.headers
    assert response.headers["Location"].startswith("https://")
    
    log_output = runnable_server_instance.get_output()
    assert log_output.count("client_redirected=true") == 1
    
def test_redirection_server_always_closes_connection(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that the redirection server always closes the connection after redirecting,
    regardless of the client's Connection header.
    """
    # GIVEN
    REDIRECTION_PORT = 8081
    
    _setup_config(runnable_server_instance)
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()

    # WHEN
    conn = HTTPConnection("127.0.0.1", REDIRECTION_PORT, timeout=2)

    conn.request("GET", "/", headers={"Connection": "keep-alive"})
    r1 = conn.getresponse()
    r1.read() 
    
    assert runnable_server_instance.get_output().count("event=client_disconnected") == 1

    with pytest.raises(RemoteDisconnected):
        conn.request("GET", "/")
        r2 = conn.getresponse()
        r2.read() 

    conn.close()
    
    # THEN
    assert r1.status == 301
    
def test_redirection_server_includes_port_when_https_port_is_not_443(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that the redirection server includes the correct port in the Location header
    when redirecting, especially when the HTTPS server is running on a non-standard port.
    """
    # GIVEN
    REDIRECTION_PORT = 8081
    
    _setup_config(runnable_server_instance)
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()

    # WHEN
    runnable_server_instance.wait_for_output("event=redirection_server_running")
    response, body = _make_request("GET", "/", {"Connection": "close"}, port=REDIRECTION_PORT)

    # THEN
    assert response.status == 301
    assert "Location" in response.headers
    assert response.headers["Location"].startswith("https://")
    assert ":8080" in response.headers["Location"]
    
def test_redirection_server_does_not_include_port_when_https_port_is_443(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that the redirection server does not include the port in the Location header
    when redirecting if the HTTPS server is running on the standard port 443.
    """
    # GIVEN
    REDIRECTION_PORT = 8081
    
    _setup_config(runnable_server_instance)
    runnable_server_instance.set_config_value("ports.https_port", 443)
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()

    # WHEN
    runnable_server_instance.wait_for_output("event=redirection_server_running")
    response, body = _make_request("GET", "/", {"Connection": "close"}, port=REDIRECTION_PORT)

    # THEN
    assert response.status == 301
    assert "Location" in response.headers
    assert response.headers["Location"].startswith("https://")
    assert ":443" not in response.headers["Location"]

def test_redirection_server_handles_malformed_request_method_returns_400_or_ge_400(
    runnable_server_instance: HttpServerRunner,
):
    """
    Send an invalid/malformed request line to the server and assert a 4xx response.
    The exact status may vary (400 is expected), but ensure it's a client error.
    """
    # GIVEN
    REDIRECTION_PORT = 8081
    
    _setup_config(runnable_server_instance)
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()

    # WHEN
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(2.0)
    try:
        s.connect(("127.0.0.1", REDIRECTION_PORT))
        s.sendall(b"badmethod / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
        # Try to read the status line from response
        data = s.recv(1024).decode(errors="ignore")
    finally:
        try:
            s.close()
        except Exception:
            pass

    # THEN
    assert "HTTP/1.1" in data or "HTTP/1.0" in data
    assert "400 Bad Request" in data