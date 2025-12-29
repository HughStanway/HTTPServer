import socket
import threading
import time
from conftest import HttpServerRunner
from common import _make_request


def test_server_handles_requests_concurrently(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that multiple clients are handled concurrently via the thread pool
    rather than sequentially.
    """
    # GIVEN
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    REQUEST_COUNT = 8

    # WHEN
    start = time.monotonic()

    threads = [
        threading.Thread(
            target=_make_request, args=("GET", "/slow", {"Connection": "close"})
        )
        for _ in range(REQUEST_COUNT)
    ]

    for t in threads:
        t.start()

    for t in threads:
        t.join()

    elapsed = time.monotonic() - start

    # THEN
    # Sequential time would be ~ REQUEST_COUNT * SLEEP_MS = 4s
    # Parallel time should be closer to ceil(REQUEST_COUNT / WORKER_COUNT) * SLEEP_MS = ~1s
    assert elapsed < 2, f"Requests were handled sequentially (elapsed={elapsed:.2f}s)"

    log_output = runnable_server_instance.get_output()
    assert log_output.count("Accepted client") >= REQUEST_COUNT
