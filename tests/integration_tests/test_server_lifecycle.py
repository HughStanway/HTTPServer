import socket
import time
import threading
from conftest import HttpServerRunner


def test_startup_and_gracefull_shutdown(
    runnable_server_instance: HttpServerRunner
):
    """
    Test verifies that the server framework starts and stops gracfully
    on SIGTERM ensuring that all threads and connections are closed before
    exiting.
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    
    # WHEN:
    runnable_server_instance.stop()
    
    # THEN:
    assert not runnable_server_instance.is_alive()
    
    log_output = runnable_server_instance.get_output()
    assert "SIGINT or SIGTERM received, shutting down ..." in log_output
    assert "Shutdown: All client threads finished." in log_output
    assert "Shutdown: Server main loop exited." in log_output
    assert "Server exited cleanly" in log_output
    assert runnable_server_instance.exit_code() == 0
    
   
def test_shutdown_waits_for_connected_clients_to_disconnect_gracefully(
    runnable_server_instance: HttpServerRunner
):
    """
    Test verifies that the server framework waits for currently connected clients
    to disconnect (close connection or timeout) before exiting.
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    host = "127.0.0.1"
    port = 8080

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(2.0)
    try:
        s.connect((host, port))
    except Exception as e:
        s.close()
        raise AssertionError(f"Failed to connect client socket to test server: {e}")

    assert runnable_server_instance.wait_for_output("Accepted client")
    
    # Send a simple request and keep the connection open
    req = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n"
    s.sendall(req.encode())
    
    # WHEN:
    # call stop in a background thread because stop() is blocking
    stop_thread = threading.Thread(target=runnable_server_instance.stop, daemon=True)

    t0 = time.monotonic()
    stop_thread.start()
    stop_thread.join(timeout=10)
    elapsed = time.monotonic() - t0

    # Ensure stop returned (thread joined). If it didn't, we must stop the test.
    assert not stop_thread.is_alive(), "stop() didn't finish within timeout; test failed."

    # THEN:
    # Because the server will wait for client timeouts / client thread exit we expect
    # stop() to block for at least a few seconds (idle timeout configured in server)
    assert elapsed >= 4.5, f"stop() returned too quickly: {elapsed:.3f}s but expected >= 4.0s"

    assert runnable_server_instance.exit_code() == 0

    log_output = runnable_server_instance.get_output()
    assert "idle timeout reached, closing"
    assert "Shutdown: All client threads finished." in log_output
    assert "Shutdown: Server main loop exited." in log_output
    assert "Server exited cleanly" in log_output

    # Cleanup the client socket
    try:
        s.close()
    except Exception:
        pass
   

def test_server_disconnects_client_after_timeout(runnable_server_instance: HttpServerRunner):
    """
    Test verifies that clients that dont close the connection are disconnected by the server
    after a preset 5 second time out if no requests are sent
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    
    s = socket.create_connection(("127.0.0.1", 8080), timeout=2)
    assert runnable_server_instance.wait_for_output("Accepted client")
    
    # WHEN:
    time.sleep(6)
    
    # THEN:
    log_output = runnable_server_instance.get_output()
    assert "idle timeout reached, closing" in log_output
    assert "disconnected" in log_output
