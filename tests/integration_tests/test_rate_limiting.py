from conftest import HttpServerRunner
from common import _make_request
from concurrent.futures import ThreadPoolExecutor
import threading

def test_connection_guard_tracks_client_lifecycle(
    runnable_server_instance: HttpServerRunner
):
    """
    Test verifies that a client connection correctly enters and exits
    the connection guard, incrementing and decrementing the active
    connection count.
    """
    # GIVEN
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    
    # WHEN
    _, _ = _make_request("GET", "/", {"Connection": "close"})
     
    # THEN
    log_output = runnable_server_instance.get_output()
    assert "event=connection_guard_enter" in log_output
    assert "active_connections=1" in log_output
    assert "event=connection_guard_exit" in log_output
    assert "active_connections=0" in log_output

def test_server_limits_concurrent_connections_per_ip(
    runnable_server_instance: HttpServerRunner
):
    """
    Test verifies that the server enforces a limit on concurrent connections
    from the same IP address, rejecting excess connections with a 503 response.
    """
    # GIVEN
    runnable_server_instance.set_config_value("rate-limits.max_connections_per_ip", 1)
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN
    threads = []
    responses = []

    for _ in range(2):
        t = threading.Thread(
            target=lambda: responses.append(
                _make_request("GET", "/slow", {"Connection": "close"})
            )
        )
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    # THEN
    success_count = sum(1 for r in responses if r[0].status == 200)
    reject_count = sum(1 for r in responses if r[0].status == 503)

    assert success_count == 1
    assert reject_count == 1

    log_output = runnable_server_instance.get_output()
    assert log_output.count("event=connection_accepted") == 2
    assert log_output.count("event=max_number_connections_from_ip_exceeded") == 1
    assert log_output.count("event=connected") == 1
