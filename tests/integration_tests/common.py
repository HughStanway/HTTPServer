import ssl
from http.client import HTTPConnection, HTTPSConnection


def _make_request(
    method: str,
    endpoint: str,
    headers=None,
    host="localhost",
    port=8080,
    use_https=False,
):
    headers = headers or {}

    if use_https:
        context = ssl.create_default_context()
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE

        conn = HTTPSConnection(
            host,
            port,
            timeout=2,
            context=context,
        )
    else:
        conn = HTTPConnection(host, port, timeout=2)

    conn.request(method, endpoint, headers=headers)
    resp = conn.getresponse()
    body = resp.read().decode("utf-8")

    tls_info = None
    if use_https:
        sock = conn.sock
        tls_info = {
            "cipher": sock.cipher(),
            "version": sock.version(),
            "peercert": sock.getpeercert(),
        }

    conn.close()
    return (resp, body, tls_info) if use_https else (resp, body)