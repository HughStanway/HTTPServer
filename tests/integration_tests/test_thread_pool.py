import threading
import time

from common import _make_request
from conftest import HttpServerRunner


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
    # Parallel time should be closer to
    # ceil(REQUEST_COUNT / WORKER_COUNT) * SLEEP_MS = ~1s
    assert elapsed < 1.2, f"Requests were handled sequentially (elapsed={elapsed:.2f}s)"

    log_output = runnable_server_instance.get_output()
    assert log_output.count("event=connection_accepted") == REQUEST_COUNT


def test_thread_pool_rejects_requests_when_queue_full(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that when the thread pool queue is full, additional requests are rejected
    with a 503 response.
    """
    # GIVEN
    WORKER_COUNT = 1
    QUEUE_SIZE = 1
    TOTAL_REQUESTS = WORKER_COUNT + QUEUE_SIZE + 2  # Exceed capacity by 2

    runnable_server_instance.set_config_value("threading.min_threads", WORKER_COUNT)
    runnable_server_instance.set_config_value("threading.max_threads", WORKER_COUNT)
    runnable_server_instance.set_config_value("threading.max_queue_size", QUEUE_SIZE)

    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    assert runnable_server_instance.wait_for_output(
        f"event=thread_pool_started worker_threads={WORKER_COUNT}"
    )

    # WHEN
    threads = []
    responses = []

    for _ in range(TOTAL_REQUESTS):
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
    success_responses = [r for r in responses if r[0].status == 200]
    error_responses = [r for r in responses if r[0].status == 503]

    assert len(success_responses) == WORKER_COUNT + QUEUE_SIZE
    assert len(error_responses) == TOTAL_REQUESTS - (WORKER_COUNT + QUEUE_SIZE)

    log_output = runnable_server_instance.get_output()
    assert log_output.count("event=thread_pool_queue_limit_reached") == 2
