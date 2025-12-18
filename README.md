# HTTPServer

![License](https://img.shields.io/github/license/HughStanway/HTTPServer)
![Stars](https://img.shields.io/github/stars/HughStanway/HTTPServer)
![Tests](https://img.shields.io/github/actions/workflow/status/HughStanway/HTTPServer/build-and-test.yml?branch=main)
![Release](https://img.shields.io/github/v/release/HughStanway/HTTPServer)

A small, single-binary HTTP server library and example application written in modern C++ (C++20).
It provides a minimal HTTP request parser, routing, static-file handling, and helpers for building HTTP responses. The repository includes a test suite for the parser.

## Directory Structure

- `lib/` — library source. Headers are under `lib/include/httpserver/` and implementations under `lib/src/`.
- `src/` — example HTTP server implemetation using the library (`src/main.cpp`).
- `public/` — example static site to serve (HTML/CSS/JS).
- `tests/` — unit tests (uses GoogleTest) and integration tests (uses Pytest).
- `Makefile` — build and developer convenience targets.

## Key components and API structure

- Server: `server.h` — lifecycle (start/stop), signal handling, and socket management.
- Threading: the server accepts connections and dispatches each client to a worker thread (thread-per-connection) with lightweight pooling and join semantics implemented in `lib/src/server.cpp`.
- Request parsing: `http_parser.h` — parsing request line, headers, and body into `HttpRequest` objects.
- HTTP objects: `http_object.h` - defines HttpRequest and HttpReponse objects for representing requests and responses.
- Router: `router.h` — API to register handlers and dispatch requests to application callbacks.
- Response helpers: `http_response_builder.h` - for constructing response objects.
- Utilities: `utils.h` - for MIME-type lookup, keep-alive logic, and helpers in.
- Logger: `logger.h` - for lightweight logging implementation.

Refer to the headers in `lib/include/httpserver/` for data types and function signatures.

## Building

This project uses CMake; a `Makefile` found in the project root offers shortcuts for common commands.

Using the Makefile shortcuts:

```bash
make build            # configure and build in ./build
make run              # build then run the example server
make clean            # Removes the current build
make unit_test        # Build and run only the unit tests
make integration_test # Build and run only the integration tests
make test             # build and run tests
make format           # run clang-format over sources (if available)
```

Or use CMake directly:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Notes:

- Tests use GoogleTest and are added via CMake.
- Use `-DENABLE_SANITIZERS=ON` when configuring to enable sanitizers (if supported by your toolchain).

## Running the example server

After building with `make build`, the example binary is generated at `build/src/http_server`. By default it binds to port 443 using HTTPS and redirects HTTP requests on port 80. First create a `.env` directory in the root and generate a valid  `cert.pem` and `key.pem` file. Then, run:

```bash
./build/src/http_server
```

Then try (Note the -k flag is used if the certificates are self signed):

```bash
curl -k https://localhost/
curl -k https://localhost/add?x=1&y=2
```

See `src/main.cpp` for an example of registered routes handlers.

## Tests

Run unit tests after building the makefile targets or with:

```bash
ctest --test-dir build
```

or run the test binary directly:

```bash
./build/tests/test_httpserver
```

and run integration tests with:

```bash
python -m pytest -s tests/
```

Add new test modules under `tests/` and update `tests/CMakeLists.txt` as needed.

## Contributing

Fork, create a new branch, and open a pull request. You must include unit tests and integration tests where possible for new behavior and ensure the project builds on CI.

## License

This project uses the MIT license. Please refer to the `LICENSE` within the project root for more detail.
