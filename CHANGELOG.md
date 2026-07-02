# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-07-02

First public release.

### Added
- `ChecksumValidator` library: streaming file digest computation and
  constant-time verification (`CRYPTO_memcmp`) for any OpenSSL-provided
  algorithm (SHA-1/2/3, BLAKE2, MD5, RIPEMD-160, ...).
- Friendly algorithm aliases (`sha-256`, `SHA256`, `sha256` all accepted).
- CLI with `--file`, `--string`, `--algo`, `--verify`, `--chunk-size`,
  `--list-algos`, `--version`, and `--help` options.
- Documented exit codes (`0` success, `1` verification failed, `2` error).
- CMake build system with an installable static library, public headers, and a
  `pkg-config` file (`cksumx.pc`).
- Self-checking test suite that cross-validates digests against the OpenSSL CLI
  and `sha256sum` (coreutils), including streaming correctness and CLI error
  paths.
- OpenSSL 3.x provider loading (default + legacy), with handles retained for the
  program lifetime so legacy digests stay available.

[Unreleased]: https://github.com/mmohamedkhaled/cksumx/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/mmohamedkhaled/cksumx/releases/tag/v1.0.0
