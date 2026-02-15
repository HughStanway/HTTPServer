from conftest import HttpServerRunner
from common import _make_request

def test_idle_ip_cleanup_removes_inactive_ips(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that the periodic idle IP cleanup task correctly identifies and removes
    IPs that have been inactive for longer than the configured timeout and removes them
    from the server's internal tracking.
    """
    # GIVEN
    runnable_server_instance.set_config_value("idle-ip-cleanup.interval_seconds", 5)
    runnable_server_instance.set_config_value("idle-ip-cleanup.idle_timeout_seconds", 1)

    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN
    runnable_server_instance.wait_for_output("event=idle_ip_cleanup_sleeping")
    _make_request("GET", "/", {"Connection": "close"})
    
    # THEN
    assert runnable_server_instance.wait_for_output("event=idle_ip_cleanup_finished", timeout=6.0)
    
    log_output = runnable_server_instance.get_output()
    assert "event=idle_ip_entry_erased" in log_output
    assert "removed=1" in log_output

def test_idle_ip_cleanup_does_not_remove_active_ips(
    runnable_server_instance: HttpServerRunner,
):
    """
    Verifies that the periodic idle IP cleanup task does not remove IPs that have been
    active within the configured idle timeout, ensuring that active clients are not
    mistakenly disconnected.
    """
    # GIVEN
    runnable_server_instance.set_config_value("idle-ip-cleanup.interval_seconds", 5)
    runnable_server_instance.set_config_value("idle-ip-cleanup.idle_timeout_seconds", 60)

    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()

    # WHEN
    runnable_server_instance.wait_for_output("event=idle_ip_cleanup_sleeping")
    _make_request("GET", "/", {"Connection": "close"})
    
    # THEN
    assert runnable_server_instance.wait_for_output("event=idle_ip_cleanup_finished", timeout=6.0)
    
    log_output = runnable_server_instance.get_output()
    assert "event=idle_ip_entry_erased" not in log_output
    assert "removed=0" in log_output
