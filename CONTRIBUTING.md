# Contributing to cksumx

Thanks for your interest in improving cksumx! This document covers everything
you need to get a change merged.

## Code of conduct

By participating you agree to uphold the [Code of Conduct](CODE_OF_CONDUCT.md).
Please be kind and constructive.

## Prerequisites

- A C++17 compiler (g++ ≥ 9, clang++ ≥ 10, or MSVC ≥ 19.20)
- CMake ≥ 3.16
- OpenSSL ≥ 1.1.0 development headers (`libssl-dev` on Debian/Ubuntu,
  `openssl-devel` on Fedora, `brew install openssl` on macOS)
- The `openssl` CLI and `sha256sum` for running the test suite

## Getting started

```bash
git clone https://github.com/mmohamedkhaled/cksumx.git
cd cksumx
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCKSUMX_WARNINGS_AS_ERRORS=ON
cmake --build build -j
./tests/run_tests.sh
```

## Code style

- **C++17.** Avoid compiler extensions (`-std=c++17`, not `gnu++17`).
- The repository ships a `.clang-format`. Run it on everything you touch:

  ```bash
  clang-format -i src/**/*.{cpp,hpp} include/**/*.hpp
  ```

- Keep the public API in `include/cksumx/`. Implementation details belong under
  `src/` and must not be included by external consumers.
- Document every public class and method with `///` Doxygen comments
  (`\param`, `\return`, `\throws`), matching the existing style.
- Prefer RAII and the existing exception hierarchy (`ChecksumError` and
  subclasses) over manual resource management and ad-hoc error codes.

## Before you open a pull request

1. **Format** your code with `clang-format`.
2. **Build** cleanly with `CKSUMX_WARNINGS_AS_ERRORS=ON` — there must be no
   warnings.
3. **Test**: `./tests/run_tests.sh` must report `All checks passed.` with no
   failures. If your change adds behavior, extend the suite in
   `tests/run_tests.sh` or add a sample under `tests/samples/`.
4. **Commit messages**: use a concise imperative summary
   (`Add SHA-3 alias resolution`, not `added sha3`). Reference issues in the
   body when relevant.

## Pull request checklist

- [ ] Builds without warnings (`-DCKSUMX_WARNINGS_AS_ERRORS=ON`)
- [ ] `./tests/run_tests.sh` passes
- [ ] `clang-format` applied
- [ ] Public API changes documented and reflected in the README if user-facing
- [ ] No secrets, absolute paths, or machine-specific artifacts committed

## Reporting bugs

Open a [GitHub issue](https://github.com/mmohamedkhaled/cksumx/issues) and
include:

- cksumx version (`cksumx --version`) and OpenSSL version (`openssl version`)
- OS / compiler / CMake version
- The exact command and the full output
- Expected vs. actual behavior

Security-sensitive reports should follow [SECURITY.md](SECURITY.md) instead of a
public issue.
