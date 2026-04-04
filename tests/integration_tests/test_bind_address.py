import socket
import pytest
import requests
from requests.exceptions import ConnectionError, Timeout
from conftest import HttpServerRunner

def get_lan_ip() -> str:
    # Get the machine's primary local network IP address.
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # Doesn't have to be reachable
        s.connect(('10.255.255.255', 1))
        ip = s.getsockname()[0]
    except Exception:
        ip = '127.0.0.1'
    finally:
        s.close()
    return ip

def test_bind_address_isolation_rejects_external_connections(
    runnable_server_instance: HttpServerRunner
) -> None:
    """
    Verify that the server rejects external connections when bound to a specific IP.
    """
    # GIVEN
    lan_ip = get_lan_ip()
    if lan_ip == '127.0.0.1':
        pytest.skip("No external network interface available to test isolation")

    runnable_server_instance.set_config_value("ports.bind_address", "127.0.0.1")
    runnable_server_instance.set_config_value("ports.server_port", 8080)
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN and THEN
    # Localhost connection should work
    res = requests.get("http://127.0.0.1:8080/", timeout=0.5)
    assert res.status_code in [200, 404]

    # Connect via LAN IP should error out since it's strictly bound
    with pytest.raises((ConnectionError, Timeout)):
        requests.get(f"http://{lan_ip}:8080/", timeout=0.5)
