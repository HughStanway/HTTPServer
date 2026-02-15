from http.client import HTTPConnection
from conftest import HttpServerRunner
from common import _make_request
import threading
import time

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
    
def test_server_limits_number_of_requests_per_connection(
    runnable_server_instance: HttpServerRunner
):
    """
    Test verifies that the server enforces a limit on the number of requests
    that can be made over a single connection, rejecting excess requests with
    a 503 response.
    """
    # GIVEN
    runnable_server_instance.set_config_value("rate-limits.max_keep_alive_requests", 1)
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN
    conn = HTTPConnection("127.0.0.1", 8080, timeout=2)

    conn.request("GET", "/")
    r1 = conn.getresponse()
    r1.read() 

    conn.request("GET", "/")
    r2 = conn.getresponse()
    r2.read() 

    conn.close()

    # THEN
    assert r1.status == 200
    assert r2.status == 429

    log_output = runnable_server_instance.get_output()
    assert log_output.count("event=http_request") == 2
    assert log_output.count("event=max_keep_alive_requests_exceeded") == 1

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
    
def test_rate_per_ip_token_bucket_allows_burst_requests(
    runnable_server_instance: HttpServerRunner
):
    """
    Test verifies that the token bucket algorithm for rate limiting allows
    burst requests up to the configured max tokens, and then enforces the
    refill rate.
    """
    # GIVEN
    runnable_server_instance.set_config_value("rate-limits.max_tokens", 3.0)
    runnable_server_instance.set_config_value("rate-limits.refill_rate", 0.1)
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN
    responses = []
    for _ in range(4):
        response, _ = _make_request("GET", "/", {"Connection": "close"})
        responses.append(response.status)

    # THEN
    assert responses[:3] == [200, 200, 200]
    assert responses[3] == 429

    log_output = runnable_server_instance.get_output()
    assert log_output.count("event=connected") == 4
    assert "event=rate_limit_exceeded" in log_output


def test_rate_per_ip_token_bucket_refills_over_time(
    runnable_server_instance: HttpServerRunner
):
    """
    Test verifies that after the token bucket is exhausted, requests are
    accepted again once enough time has passed for refill.
    """
    # GIVEN
    runnable_server_instance.set_config_value("rate-limits.max_tokens", 1.0)
    runnable_server_instance.set_config_value("rate-limits.refill_rate", 5.0)
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN
    r1, _ = _make_request("GET", "/", {"Connection": "close"})
    r2, _ = _make_request("GET", "/", {"Connection": "close"})
    time.sleep(0.25)
    r3, _ = _make_request("GET", "/", {"Connection": "close"})

    # THEN
    assert r1.status == 200
    assert r2.status == 429
    assert r3.status == 200
    log_output = runnable_server_instance.get_output()
    print(log_output)
