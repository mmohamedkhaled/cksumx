# Security policy

## Reporting a vulnerability

Please **do not** open a public GitHub issue for security problems.

Instead, report vulnerabilities privately:

1. Open a **private security advisory** via
   [GitHub's security advisories page](https://github.com/mmohamedkhaled/cksumx/security/advisories/new),
   **or**
2. Email the maintainer at **mmohamedkhaled@users.noreply.github.com** with `[cksumx
   security]` in the subject.

Include a description of the issue, steps to reproduce, and the expected impact.
You should receive an initial response within **72 hours**.

## Scope

cksumx wraps OpenSSL's libcrypto for digest computation and verification. The
following are considered in scope:

- Flaws in cksumx's own logic (e.g. incorrect verification, memory-safety bugs,
  resource exhaustion).
- Issues introduced by this project's build or packaging.

The following are **out of scope** and should be reported to the
[OpenSSL project](https://www.openssl.org/community/#security):

- Vulnerabilities in OpenSSL/libcrypto itself.
- Weaknesses inherent to a specific digest algorithm (e.g. MD5/SHA-1 collision
  resistance) — these are documented design trade-offs, see the README.

## Supported versions

Only the latest release line receives security fixes.

| Version | Supported          |
| ------- | ------------------ |
| 1.x     | :white_check_mark: |
| < 1.0   | :x:                |

## Disclosure

We follow coordinated disclosure: a fix is prepared privately, a release is
cut, and the advisory is published along with credit to the reporter (unless
they prefer to remain anonymous).
