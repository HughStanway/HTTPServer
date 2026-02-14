from conftest import HttpServerRunner
from common import _make_request

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
    