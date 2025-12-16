import pytest # type: ignore
import socket
import ssl
from conftest import HttpServerRunner
from common import _make_request


def test_client_connection_to_wrong_port_refused(runnable_server_instance: HttpServerRunner):
    """
    Verifies that a connection to a non-bound server port is refused
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    
    # WHEN and THEN:
    with pytest.raises(ConnectionRefusedError):
        _, _, = _make_request("GET", "/", port=8000)


def test_http_fails_with_https_enabled_and_no_redirection(runnable_server_instance: HttpServerRunner):
    """
    Verifies that when server is running with HTTPS enabled it will not accept requests
    over HTTP
    """
    # GIVEN:
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()
    
    # WHEN and THEN:
    with pytest.raises(ConnectionResetError):
        _, _, = _make_request("GET", "/", port=8443)

    assert "TLS handshake failed" in runnable_server_instance.get_output()
    
    
def test_https_request_fails_with_https_disabled(runnable_server_instance: HttpServerRunner):
    """
    Verifies that when the server is running with only HTTP it will not accept requests
    over HTTPS
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    
    # WHEN and THEN:
    with pytest.raises(ssl.SSLError):
        _make_request("GET", "/", use_https=True)
    

def test_malformed_request_method_returns_400_or_ge_400(runnable_server_instance: HttpServerRunner):
    """
    Send an invalid/malformed request line to the server and assert a 4xx response.
    The exact status may vary (400 is expected), but ensure it's a client error.
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(2.0)
    try:
        s.connect(('127.0.0.1', 8080))
        s.sendall(b"badmethod / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
        # Try to read the status line from response
        data = s.recv(1024).decode(errors='ignore')
    finally:
        try:
            s.close()
        except Exception:
            pass
   
    # THEN: 
    assert "HTTP/1.1" in data or "HTTP/1.0" in data
    assert "400 Bad Request" in data