#!/usr/bin/env bash
#
# Test harness for cksumx.
#
# Strategy: the OpenSSL CLI (openssl dgst) is used as a live oracle. For every
# (sample file x algorithm) pair we compare our tool's digest against the
# oracle. SHA-256 is additionally cross-checked against an independent oracle
# (sha256sum from coreutils). The CLI's functional and error-handling paths
# are then exercised through their documented exit codes.

set -u

# --- setup -------------------------------------------------------------------

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SAMPLES_DIR="$ROOT/tests/samples"
BUILD_DIR="$ROOT/build"

# Allow pointing the suite at a pre-built binary; otherwise build via CMake.
BIN="${CKSUMX_BIN:-${CHECKSUM_TOOL:-}}"
# Extra arguments forwarded to the CMake configure step (e.g. -DOPENSSL_ROOT_DIR=...).
CMAKE_ARGS="${CKSUMX_CMAKE_ARGS:-}"

ALGOS=(sha256 sha1 sha512 md5 sha3-256 sha3-512 blake2b512 ripemd160)

PASS=0
FAIL=0
SKIP=0
SKIPPED_CASES=()
FAILED_CASES=()

note_pass() { PASS=$((PASS + 1)); }
note_skip() {
    SKIP=$((SKIP + 1))
    SKIPPED_CASES+=("$1")
}
note_fail() {
    FAIL=$((FAIL + 1))
    FAILED_CASES+=("$1")
    echo "  FAIL: $1"
}

# --- build (only if no binary was supplied) ---------------------------------

if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
    if [ -z "$BIN" ]; then
        BIN="$BUILD_DIR/cksumx"
    fi
    echo "Configuring & building via CMake ..."
    if ! cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release $CMAKE_ARGS >/dev/null; then
        echo "error: CMake configuration failed (is OpenSSL installed?)" >&2
        exit 2
    fi
    if ! cmake --build "$BUILD_DIR" -j >/dev/null; then
        echo "error: failed to build cksumx" >&2
        exit 2
    fi
fi

mapfile -t SAMPLES < <(find "$SAMPLES_DIR" -type f -name '*.txt' | sort)

if [ "${#SAMPLES[@]}" -eq 0 ]; then
    echo "error: no sample files found in $SAMPLES_DIR" >&2
    exit 2
fi

# --- oracle ------------------------------------------------------------------

# Print the digest of $1 (algorithm) over file $2 using OpenSSL.
# Falls back to explicitly loading the legacy provider, which OpenSSL 3.x uses
# for RIPEMD-160 / Whirlpool on strict builds.
openssl_oracle() {
    local out
    out="$(openssl dgst "-$1" "$2" 2>/dev/null | awk '{print $NF}')"
    if [ -z "$out" ]; then
        out="$(openssl dgst "-$1" -provider default -provider legacy "$2" 2>/dev/null \
            | awk '{print $NF}')"
    fi
    printf '%s' "$out"
}

# Independent second opinion for sha256 (coreutils).
sha256sum_oracle() {
    sha256sum "$1" 2>/dev/null | awk '{print $1}'
}

# --- 1. digest parity vs openssl, across all samples and algorithms ----------
echo "==> [1/6] Digest parity vs openssl (${#SAMPLES[@]} samples x ${#ALGOS[*]} algorithms)"
for f in "${SAMPLES[@]}"; do
    for algo in "${ALGOS[@]}"; do
        expected="$(openssl_oracle "$algo" "$f")"
        if [ -z "$expected" ]; then
            note_skip "$algo not available in oracle ($f)"
            continue
        fi
        got="$("$BIN" -f "$f" -a "$algo")" || true
        if [ "$got" = "$expected" ]; then
            note_pass
        else
            note_fail "$algo $f (got=$got want=$expected)"
        fi
    done
done

# --- 2. independent sha256 cross-check vs sha256sum --------------------------
echo "==> [2/6] Independent SHA-256 cross-check vs sha256sum"
for f in "${SAMPLES[@]}"; do
    want="$(sha256sum_oracle "$f")"
    got="$("$BIN" -f "$f" -a sha256)" || true
    if [ "$got" = "$want" ]; then
        note_pass
    else
        note_fail "sha256sum $f (got=$got want=$want)"
    fi
done

# --- 3. streaming correctness with a 1-byte buffer ---------------------------
echo "==> [3/6] Streaming correctness (chunk-size 1 byte)"
for f in "${SAMPLES[@]}"; do
    want="$(openssl_oracle sha256 "$f")"
    got="$("$BIN" -f "$f" -a sha256 --chunk-size 1)" || true
    if [ "$got" = "$want" ]; then
        note_pass
    else
        note_fail "chunk-size 1 $f (got=$got want=$want)"
    fi
done

# --- 4. verify: matching and non-matching digests ----------------------------
echo "==> [4/6] Verify path (match and mismatch)"
f="${SAMPLES[0]}"
good="$("$BIN" -f "$f" -a sha256)"
if "$BIN" -f "$f" -a sha256 -v "$good" >/dev/null 2>&1; then
    note_pass
else
    note_fail "verify match should exit 0"
fi
# wrong-length digest must also be rejected.
if "$BIN" -f "$f" -a sha256 -v "$(printf '0%.0s' {1..63})" >/dev/null 2>&1; then
    note_fail "verify mismatch wrongly accepted"
else
    note_pass
fi

# --- 5. string mode ----------------------------------------------------------
echo "==> [5/6] String mode vs printf|openssl"
want="$(printf '%s' "hello" | openssl_oracle sha256 /dev/stdin)"
got="$("$BIN" -s "hello" -a sha256)" || true
if [ "$got" = "$want" ]; then
    note_pass
else
    note_fail "string mode (got=$got want=$want)"
fi

# --- 6. error handling / exit codes ------------------------------------------
echo "==> [6/6] Error handling"
"$BIN" -f "$SAMPLES_DIR/does_not_exist.txt" >/dev/null 2>&1
[ $? -eq 2 ] && note_pass || note_fail "missing file should exit 2"
"$BIN" -f "$SAMPLES_DIR" >/dev/null 2>&1   # a directory
[ $? -eq 2 ] && note_pass || note_fail "directory should exit 2"
"$BIN" -f "$f" -a not-a-real-algorithm >/dev/null 2>&1
[ $? -eq 2 ] && note_pass || note_fail "bad algorithm should exit 2"
"$BIN" -f "$f" --chunk-size 0 >/dev/null 2>&1
[ $? -eq 2 ] && note_pass || note_fail "chunk-size 0 should exit 2"
"$BIN" >/dev/null 2>&1                      # no arguments
[ $? -eq 2 ] && note_pass || note_fail "no args should exit 2"

# --- summary -----------------------------------------------------------------
echo
echo "--------------------------------------------------"
echo "PASS=$PASS  FAIL=$FAIL  SKIP=$SKIP  total=$((PASS + FAIL + SKIP))"
if [ "$SKIP" -ne 0 ]; then
    echo "Skipped (oracle unavailable, not a failure):"
    printf '  ~ %s\n' "${SKIPPED_CASES[@]}"
fi
if [ "$FAIL" -ne 0 ]; then
    echo "Failed cases:" >&2
    printf '  - %s\n' "${FAILED_CASES[@]}" >&2
    exit 1
fi
echo "All checks passed."
exit 0
