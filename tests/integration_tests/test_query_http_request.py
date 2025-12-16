from conftest import HttpServerRunner
from common import _make_request


def test_http_request_with_simple_parameters(runnable_server_instance: HttpServerRunner):
    """
    Verify server correctly parses and returns a single parameter
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    
    param = "test"
    
    # WHEN:
    reponse, body = _make_request("GET", f"/param?input={param}")
    
    # THEN:
    assert reponse.status == 200
    assert f"Parameter: {param}" in body
    

def test_http_request_with_parameters_and_url_decoding(runnable_server_instance: HttpServerRunner):
    """
    Verify server correctly parses and decodes more complex URL encoded params and 
    returns them in decoded format
    """
    # GIVEN:
    runnable_server_instance.start()
    assert runnable_server_instance.is_alive()
    
    param = "hello%20world%21"
    
    # WHEN:
    response, body = _make_request("GET", f"/param?input={param}")
    
    # THEN:
    assert response.status == 200
    assert f"Parameter: hello world!" in body