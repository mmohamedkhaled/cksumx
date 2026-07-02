# cksumx

[![CI](https://github.com/mmohamedkhaled/cksumx/actions/workflows/ci.yml/badge.svg)](https://github.com/mmohamedkhaled/cksumx/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](https://en.cppreference.com/w/cpp/17)
[![OpenSSL](https://img.shields.io/badge/OpenSSL-%E2%89%A51.1.0-green.svg)](https://www.openssl.org/)

A small, dependency-light command-line utility **and embeddable library** for
computing and verifying cryptographic checksums of files. Built in C++17 on top
of **OpenSSL's libcrypto**, it supports the full digest suite (SHA-1, SHA-2,
SHA-3, BLAKE2, MD5, RIPEMD-160) and streams files in fixed-size binary chunks so
that arbitrarily large inputs (ISO images, database dumps, log archives) can be
hashed in constant memory.

---

## Table of contents

- [Features](#features)
- [Quick start](#quick-start)
- [Requirements](#requirements)
- [Building](#building)
- [Usage](#usage)
- [Using cksumx as a library](#using-cksumx-as-a-library)
- [Testing](#testing)
- [Exit codes](#exit-codes)
- [Docker](#docker)
- [Security notes](#security-notes)
- [Contributing](#contributing)
- [License](#license)

## Features

- **Streaming I/O** — files are read in 64 KiB chunks (configurable), so peak
  memory is independent of file size.
- **Multiple algorithms** — any digest provided by your OpenSSL build.
- **Constant-time verification** — digest comparison uses `CRYPTO_memcmp` to
  mitigate timing side-channels.
- **File or string input** — hash a file or a literal `-s/--string`.
- **Friendly algorithm aliases** — `sha-256`, `SHA256`, and `sha256` are all
  accepted.
- **Library + CLI** — `ChecksumValidator` is usable independently of the CLI;
  build with CMake, install, and link against `cksumx`.
- **Standard exit codes** — `0` success / verified, `1` verification failed,
  `2` usage or runtime error. Ideal for scripting and CI.

## Quick start

```bash
git clone https://github.com/mmohamedkhaled/cksumx.git
cd cksumx
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Smoke test
./build/cksumx -s "hello" -a sha256
# -> 2cf24dba5fb0a30e26e83b2ac5b929e1b161e5c1fa7425e73043362938b9824

# Run the test suite (requires the `openssl` CLI and `sha256sum`)
./tests/run_tests.sh
```

## Requirements

- A **C++17** compiler (g++ ≥ 9, clang++ ≥ 10, or MSVC ≥ 19.20)
- **CMake** ≥ 3.16
- **OpenSSL** ≥ 1.1.0 (libcrypto headers + library)

| Distro          | Install                                                            |
| --------------- | ----------------------------------------------------------------- |
| Debian / Ubuntu | `sudo apt install build-essential cmake libssl-dev`               |
| Fedora / RHEL   | `sudo dnf install gcc-c++ cmake openssl-devel`                    |
| macOS           | `brew install cmake openssl`                                       |

To run the test suite you also need the **OpenSSL CLI** (`openssl`) and
**coreutils** (`sha256sum`) on your `PATH`.

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The binary is produced at `build/cksumx`.

### Build options

| CMake option                  | Default | Description                          |
| ----------------------------- | ------- | ------------------------------------ |
| `CKSUMX_WARNINGS_AS_ERRORS`   | `OFF`   | Treat compiler warnings as errors.   |

On **macOS**, if CMake cannot find OpenSSL, point it at Homebrew's install:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DOPENSSL_ROOT_DIR=$(brew --prefix openssl)
```

### Installing

Prebuilt binaries for Linux and macOS are published with each
[GitHub release](https://github.com/mmohamedkhaled/cksumx/releases); each
release also includes a SHA-256 checksum file. To build and install from source:

```bash
cmake --install build        # may require sudo; honors --prefix
```

This installs the `cksumx` executable, the `libcksumx.a` static library, the
public headers under `include/cksumx/`, and a `cksumx.pc` for `pkg-config`.

## Usage

Compute a digest:

```bash
./build/cksumx --file data.txt --algo sha256
```

Verify a file against a known digest:

```bash
./build/cksumx -f data.txt -a sha256 --verify <expected_hash>
```

Hash a literal string:

```bash
./build/cksumx -s "hello" --algo sha512
```

List the algorithms available on this platform:

```bash
./build/cksumx --list-algos
```

Stream a huge file with a custom buffer size (suffixes `K`/`M`/`G` allowed):

```bash
./build/cksumx -f huge.iso -a sha256 --chunk-size 1M
```

### Options

| Option                    | Description                                            |
| ------------------------- | ------------------------------------------------------ |
| `-f, --file <path>`       | File to hash or verify.                                |
| `-a, --algo <name>`       | Hash algorithm (default: `sha256`).                    |
| `-v, --verify <hex>`      | Verify the input against this hexadecimal digest.      |
| `-s, --string <text>`     | Hash a literal string instead of a file.               |
| `--chunk-size <n>`        | Bytes read per disk operation (default: `65536`).      |
| `--list-algos`            | List available algorithms and exit.                    |
| `--version`               | Show version and exit.                                 |
| `-h, --help`              | Show help and exit.                                    |

## Using cksumx as a library

`ChecksumValidator` is reusable independently of the CLI. After `cmake --install`,
or by adding the project via CMake `FetchContent` / `add_subdirectory`:

```cmake
find_package(OpenSSL REQUIRED)
add_subdirectory(cksumx)
target_link_libraries(your_app PRIVATE cksumx::checksum)
```

```cpp
#include <cksumx/checksum_validator.hpp>

int main() {
    cksumx::ChecksumValidator v("sha256");
    std::string digest = v.compute("data.txt");
    bool ok = v.verify("data.txt", "expected_hex_digest");
}
```

`pkg-config --cflags --libs cksumx` is also available after install.

## Testing

A self-checking test suite lives in `tests/`. It builds the binary via CMake,
then uses the OpenSSL CLI as a live oracle to verify every digest across the
sample corpus and algorithm matrix, cross-checks SHA-256 against `sha256sum`
(coreutils, an independent implementation), confirms streaming correctness with
a 1-byte buffer, and exercises the CLI's verify and error-handling paths.

```bash
./tests/run_tests.sh
```

Sample inputs are in `tests/samples/` (short strings, repeated-character runs,
prose quotes, and an empty file as an edge case). The suite prints a
`PASS/FAIL` summary and exits non-zero on any failure, so it is safe to wire
into CI.

The suite is also registered with CTest, so either of these works after a build:

```bash
ctest --test-dir build --output-on-failure
cmake --build build --target test
```

To test a specific build, point it at the binary:

```bash
CKSUMX_BIN=/path/to/cksumx ./tests/run_tests.sh
```

Extra arguments can be forwarded to the CMake configure step, for example a
non-standard OpenSSL location:

```bash
CKSUMX_CMAKE_ARGS="-DOPENSSL_ROOT_DIR=/opt/openssl" ./tests/run_tests.sh
```

## Exit codes

| Code | Meaning                              |
| ---- | ------------------------------------ |
| `0`  | Success — digest computed or verified. |
| `1`  | Verification failed — digest mismatch.  |
| `2`  | Usage or runtime error.                 |

## Docker

A multi-stage `Dockerfile` builds a minimal runtime image:

```bash
docker build -t cksumx .
echo -n "hello" | docker run --rm -i cksumx -s "$(cat)" -a sha256
docker run --rm -v "$PWD":/work cksumx -f /work/data.txt -a sha256
```

## Security notes

- The default algorithm is **SHA-256**. MD5 and SHA-1 are provided for legacy
  interoperability and should **not** be used for new security-critical work.
- On OpenSSL 3.x the tool loads both the `default` and `legacy` providers and
  retains their handles for the program lifetime, so algorithms like RIPEMD-160
  and Whirlpool stay available even on strict builds where the legacy provider is
  not enabled by default.
- For tamper-resistant file integrity against an active adversary, prefer an
  HMAC over a bare digest.

Found a security issue? See [SECURITY.md](SECURITY.md) for responsible
disclosure.

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for
build instructions, code style, and the pull-request workflow.

## License

MIT — see [LICENSE](LICENSE).
