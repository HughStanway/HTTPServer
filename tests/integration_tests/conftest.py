import pytest # type: ignore
import os
import subprocess
import threading
import time
from pathlib import Path
from typing import Optional, Generator, Tuple


SERVER_BINARY = (
    Path(__file__).resolve().parents[2]
    / "build"
    / "tests"
    / "integration_tests"
    / "server_test_build"
    / "test_http_server"
)


class HttpServerRunner:
    def __init__(self, env: dict[str, str]) -> None:
        self._env = env
        self._process: Optional[subprocess.Popen[str]] = None
        self._reader_thread: Optional[threading.Thread] = None

        self._output_lines: list[str] = []
        self._output_lock = threading.Lock()

    def start(self, timeout: float = 2.0, with_https: bool = False) -> None:
        if self.is_alive():
            return
        
        if with_https:
            self._env["TEST_ENABLE_HTTPS"] = "1"

        self._process = subprocess.Popen(
            [str(SERVER_BINARY)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=self._env,
            text=True,
            errors="replace",
            bufsize=1,
        )

        assert self._process.stdout is not None

        self._reader_thread = threading.Thread(
            target=self._read_stdout,
            daemon=True,
        )
        self._reader_thread.start()

        if not self.wait_for_output("Server running", timeout):
            self.stop()
            raise RuntimeError(
                f"Server did not start within {timeout} seconds.\n"
                f"Captured output:\n{self.get_output()}"
            )

    def stop(self) -> None:
        if not self._process:
            return

        try:
            if self._process.poll() is None:
                self._process.terminate()
                try:
                    self._process.wait(timeout=6)
                except subprocess.TimeoutExpired:
                    self._process.kill()
                    self._process.wait()
        finally:
            if self._reader_thread:
                self._reader_thread.join(timeout=1)

    def _read_stdout(self) -> None:
        assert self._process is not None
        assert self._process.stdout is not None

        for line in self._process.stdout:
            with self._output_lock:
                self._output_lines.append(line)

    def wait_for_output(self, text: str, timeout: float = 2.0) -> bool:
        deadline = time.time() + timeout

        while time.time() < deadline:
            with self._output_lock:
                if any(text in line for line in self._output_lines):
                    return True

            if self._process and self._process.poll() is not None:
                break

            time.sleep(0.05)

        with self._output_lock:
            return any(text in line for line in self._output_lines)

    def is_alive(self) -> bool:
        return self._process is not None and self._process.poll() is None

    def exit_code(self) -> Optional[int]:
        return None if not self._process else self._process.poll()

    def get_output(self) -> str:
        with self._output_lock:
            return "".join(self._output_lines)


def generate_test_certs(tmpdir: Path) -> Tuple[Path, Path]:
    """Generate isolated self-signed TLS certs per test run."""
    cert = tmpdir / "cert.pem"
    key = tmpdir / "key.pem"

    subprocess.run(
        [
            "openssl", "req", "-x509", "-newkey", "rsa:2048",
            "-nodes",
            "-keyout", str(key),
            "-out", str(cert),
            "-days", "2",
            "-subj", "/CN=localhost",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    return cert, key


@pytest.fixture()
def server_temp_dir(tmp_path_factory: pytest.TempPathFactory) -> dict[str, Path]:
    """
    Creates a fully isolated filesystem layout for testing.

    Layout:
      base/
        static/
          valid_file.html
        cert.pem
        key.pem
    """
    base = tmp_path_factory.mktemp("httpserver")

    static_dir = base / "static"
    static_dir.mkdir(parents=True)

    # Create example static file
    (static_dir / "valid_file.html").write_text(
        "<h1>Simple html file contents</h1>",
        encoding="utf-8",
    )

    # Generate TLS certs
    cert, key = generate_test_certs(base)

    return {
        "base": base,
        "static_dir": static_dir,
        "cert": cert,
        "key": key,
    }

 
@pytest.fixture()
def runnable_server_instance(server_temp_dir: dict[str, Path]) -> Generator[HttpServerRunner]:
    env = os.environ.copy()
    env["TEST_ENABLE_HTTPS"] = "0"
    env["TEST_HTTPS_CERT"] = str(server_temp_dir["cert"])
    env["TEST_HTTPS_KEY"] = str(server_temp_dir["key"])
    env["TEST_STATIC_DIR"] = str(server_temp_dir["static_dir"])
    
    runner = HttpServerRunner(env=env)
    yield runner
    
    # Ensure server is always stopped
    runner.stop()
    assert not runner.is_alive()