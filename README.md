# HTTPServer

![License](https://img.shields.io/github/license/HughStanway/HTTPServer)
![Stars](https://img.shields.io/github/stars/HughStanway/HTTPServer)
![Release](https://img.shields.io/github/v/release/HughStanway/HTTPServer)
![Tests](https://img.shields.io/github/actions/workflow/status/HughStanway/HTTPServer/integration-tests.yml?branch=main)

A small, single-binary HTTP server library and example application written in modern C++ (C++20).
It provides a minimal HTTP request parser, routing, static-file handling, and helpers for building HTTP responses. The repository includes a test suite for the parser.

## Contents at a glance

- `lib/` — core library. Headers are under `lib/include/httpserver/` and implementations under `lib/src/`.
- `src/` — example binary that links the library (`src/main.cpp`).
- `public/` — example static site to serve (HTML/CSS/JS).
- `tests/` — unit tests (uses GoogleTest).
- `CMakeLists.txt`, `Makefile` — build and developer convenience targets.

## Key components and API surface

- Server: `Server` — lifecycle (start/stop), signal handling, and socket management (`lib/include/httpserver/server.h`).
- Threading: the server accepts connections and dispatches each client to a worker thread (thread-per-connection) with lightweight pooling and join semantics implemented in `lib/src/server.cpp`.
- Request parsing: `HttpParser` — parsing request line, headers, and body into `HttpRequest` (`lib/include/httpserver/http_parser.h`).
- Router: `Router` — register handlers and dispatch requests to application callbacks (`lib/include/httpserver/router.h`).
- Response helpers: `HttpResponse` and `HttpResponseBuilder` for constructing responses (`lib/include/httpserver/http_response.h`, `lib/include/httpserver/http_response_builder.h`).
- Utilities: MIME-type lookup, keep-alive logic, and helpers in `lib/include/httpserver/utils.h`.
- Logger: lightweight logging helpers in `lib/include/httpserver/logger.h`.

Refer to the headers in `lib/include/httpserver/` for data types and function signatures.

## Building

This project uses CMake; a `Makefile` offers shortcuts.

Recommended (CMake out-of-source):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Using the Makefile shortcuts:

```bash
make build    # configure and build in ./build
make run      # build then run the example server
make test     # build and run tests
make format   # run clang-format over sources (if available)
```

Notes:

- Tests use GoogleTest and are added via CMake.
- Use `-DENABLE_SANITIZERS=ON` when configuring to enable sanitizers (if supported by your toolchain).

## Running the example server

After building, the example binary is at `build/src/http_server` (or `build/src/http_server` depending on generator). By default it binds to port 8080. Run:

```bash
./build/src/http_server
```

Then try:

```bash
curl http://localhost:8080/
curl "http://localhost:8080/add?x=1&y=2"
```

See `src/main.cpp` for registered routes and example handlers.

## Tests

Run tests after building with:

```bash
ctest --test-dir build
```

or run the test binary directly:

```bash
./build/tests/test_httpserver
```

Unit tests currently cover the request parser only. Add new test modules under `tests/` and update `tests/CMakeLists.txt` as needed.

## Design notes and behavior

- Parser errors and result types are defined in the parser header; the parser reports precise error codes for invalid request lines or malformed headers.
- Header-name validation is conservative (alphanumeric, `-`, and `_`).
- Query-string parsing keeps the last value for duplicate keys.
- Connection keep-alive is determined from the `Connection` header or HTTP/1.1 defaults.
- Server handles SIGINT/SIGTERM to perform graceful shutdown via `Server::stop()`.

These behaviors are implemented in the corresponding source files under `lib/src/`.

## Development workflow

- Format with `clang-format` (Makefile target `format`).
- Static analysis via `clang-tidy` is integrated into the build/Makefile where available.
- Run individual tests during development by executing their test binary from `build/tests/`.

## Contributing

Fork, create a feature branch, and open a pull request. Include tests for new behavior and ensure the project builds on CI.

## License

No license file is currently present in the repository.

## Where to look next

- Parser: `lib/include/httpserver/http_parser.h`, `lib/src/http_parser.cpp`
- Router: `lib/include/httpserver/router.h`, `lib/src/router.cpp`
- Server: `lib/include/httpserver/server.h`, `lib/src/server.cpp`
