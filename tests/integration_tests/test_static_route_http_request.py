import pytest # type: ignore
from conftest import HttpServerRunner
from common import _make_request


def test_static_directory_route_valid_path(runnable_server_instance: HttpServerRunner):
    """
    Verifies a static route to a valid filepath returns the file contents    
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    
    # WHEN:
    response, body = _make_request("GET", "/static/valid_file.html")
    
    # THEN:
    assert response.status == 200
    assert "<h1>Simple html file contents</h1>" in body


def test_static_directory_route_invalid_path(runnable_server_instance: HttpServerRunner):
    """
    Verifies a static route to an invalid filepath returns not found    
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    
    # WHEN:
    response, body = _make_request("GET", "/static/invalid_file.html")
    
    # THEN:
    assert response.status == 404


def test_static_directory_change_file_path_is_not_allowed(runnable_server_instance: HttpServerRunner):
    """
    Verifies that static routes with .. directory changes in the filepath are not allowed 
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    
    # WHEN:
    response, body = _make_request("GET", "/static/../bad_file.txt")
    
    # THEN:
    assert response.status == 400
