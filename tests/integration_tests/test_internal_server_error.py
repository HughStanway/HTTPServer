import pytest # type: ignore
import socket
import ssl
from http.client import HTTPSConnection
from contextlib import contextmanager
from common import _make_request
from conftest import HttpServerRunner


@contextmanager
def _reserve_port(port: int):
    try:
        sock = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        sock.bind(("::", port))
    except OSError:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("127.0.0.1", port))
    sock.listen(1)
    try:
        yield
    finally:
        sock.close()


def test_startup_fails_with_invalid_tls_files(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies startup fails cleanly when HTTPS is enabled but certificate/key
    files cannot be loaded.
    """
    # GIVEN
    runnable_server_instance.set_config_value("https.cert_file", "/tmp/missing-cert.pem")
    runnable_server_instance.set_config_value("https.key_file", "/tmp/missing-key.pem")

    # WHEN / THEN
    with pytest.raises(RuntimeError):
        runnable_server_instance.start(with_https=True)

    assert not runnable_server_instance.is_alive()
    
    log_output = runnable_server_instance.get_output()
    assert "event=server_starting" in log_output
    assert "event=ssl_cert_or_key_load_failed" in log_output


def test_startup_fails_when_main_port_is_in_use(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies startup fails if the main server port is already occupied.
    """
    # GIVEN / WHEN / THEN
    with _reserve_port(8080):
        with pytest.raises(RuntimeError):
            runnable_server_instance.start()

    assert not runnable_server_instance.is_alive()
    log_output = runnable_server_instance.get_output()
    assert "event=main_socket_create_failed" in log_output


def test_redirection_start_failure_does_not_stop_main_server(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that if the redirection server cannot bind its port, the main HTTPS
    server still starts and serves traffic.
    """
    # GIVEN
    runnable_server_instance.set_config_value("https.enable_http_redirection", True)
    runnable_server_instance.set_config_value("ports.http_redirection_port", 8081)

    # WHEN
    with _reserve_port(8081):
        runnable_server_instance.start(with_https=True)
        assert runnable_server_instance.is_alive()

        response, body, _ = _make_request(
            "GET",
            "/",
            use_https=True,
        )

        # THEN
        assert response.status == 200
        assert "OK" in body
        
        log_output = runnable_server_instance.get_output()
        assert "event=redirection_server_start_failed" in log_output


def test_tls_handshake_failure_isolated_to_client(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that a failed TLS handshake closes that client only and does not
    break subsequent valid HTTPS requests.
    """
    # GIVEN
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()

    # WHEN
    bad_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    bad_client.settimeout(2.0)
    try:
        bad_client.connect(("127.0.0.1", 8080))
        bad_client.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        try:
            bad_client.recv(1024)
        except OSError:
            pass
    finally:
        bad_client.close()

    context = ssl.create_default_context()
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE

    conn = HTTPSConnection("127.0.0.1", 8080, timeout=2, context=context)
    conn.request("GET", "/", headers={"Connection": "close"})
    response = conn.getresponse()
    body = response.read().decode("utf-8")
    conn.close()

    # THEN
    assert response.status == 200
    assert "OK" in body
    
    log_output = runnable_server_instance.get_output()
    assert "event=tls_handshake_failed" in log_output


@pytest.mark.filterwarnings("ignore:ssl.TLSVersion.TLSv1 is deprecated:DeprecationWarning")
def test_https_rejects_client_using_tls_1_0(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies HTTPS server rejects clients restricted to TLS 1.0.
    """
    # GIVEN
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()

    # WHEN / THEN
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    context.minimum_version = ssl.TLSVersion.TLSv1
    context.maximum_version = ssl.TLSVersion.TLSv1

    conn = HTTPSConnection("127.0.0.1", 8080, timeout=2, context=context)
    with pytest.raises((ssl.SSLError, ConnectionResetError, TimeoutError, OSError)):
        conn.request("GET", "/", headers={"Connection": "close"})
        conn.getresponse()
    conn.close()


def test_https_rejects_disallowed_tls12_cipher(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies HTTPS server rejects TLS 1.2 clients using a cipher outside the
    configured allowlist.
    """
    # GIVEN
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()

    # WHEN / THEN
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.set_ciphers("AES128-SHA")

    conn = HTTPSConnection("127.0.0.1", 8080, timeout=2, context=context)
    with pytest.raises((ssl.SSLError, ConnectionResetError, TimeoutError, OSError)):
        conn.request("GET", "/", headers={"Connection": "close"})
        conn.getresponse()
    conn.close()
    
    log_output = runnable_server_instance.get_output()
    assert "event=tls_handshake_failed" in log_output


def test_https_accepts_allowed_tls12_cipher(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies HTTPS server accepts a TLS 1.2 client using an allowed cipher.
    """
    # GIVEN
    runnable_server_instance.start(with_https=True)
    assert runnable_server_instance.is_alive()

    # WHEN
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.set_ciphers("ECDHE-RSA-AES128-GCM-SHA256")

    conn = HTTPSConnection("127.0.0.1", 8080, timeout=2, context=context)
    conn.request("GET", "/", headers={"Connection": "close"})
    response = conn.getresponse()
    body = response.read().decode("utf-8")
    conn.close()

    # THEN
    assert response.status == 200
    assert "OK" in body
