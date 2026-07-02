# syntax=docker/dockerfile:1
#
# Multi-stage build for cksumx.
# Stage 1 builds the CLI from source; stage 2 ships a minimal runtime image.

ARG OPENSSL_VERSION=3

# --- build stage -------------------------------------------------------------
FROM ubuntu:26.04 AS build

ARG OPENSSL_VERSION

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        "libssl${OPENSSL_VERSION}" \
        libssl-dev \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DCKSUMX_WARNINGS_AS_ERRORS=ON \
    && cmake --build build -j"$(nproc)" --target cksumx_cli

# --- runtime stage -----------------------------------------------------------
FROM ubuntu:26.04 AS runtime

ARG OPENSSL_VERSION

RUN apt-get update \
    && apt-get install -y --no-install-recommends "libssl${OPENSSL_VERSION}" \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/cksumx /usr/local/bin/cksumx

# Smoke-check that the binary runs in the final image.
RUN cksumx --version

ENTRYPOINT ["cksumx"]
CMD ["--help"]
