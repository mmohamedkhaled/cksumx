// crypto_util.hpp
//
// Internal OpenSSL helpers shared by the checksum library and the CLI.
//
// This header is an implementation detail: it lives under src/ and is not
// installed alongside the public <cksumx/checksum_validator.hpp> API.

#pragma once

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#if OPENSSL_VERSION_MAJOR >= 3
#include <openssl/provider.h>
#endif

#include <memory>
#include <string>

namespace cksumx {

// RAII wrapper for EVP_MD_CTX to guarantee cleanup on all exit paths.
struct EvpMdCtxDeleter {
    void operator()(EVP_MD_CTX* ctx) const noexcept {
        if (ctx) EVP_MD_CTX_free(ctx);
    }
};
using EvpMdCtxPtr = std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter>;

// Drain the OpenSSL error stack into a single human-readable message.
inline std::string openssl_error() {
    std::string msg;
    char buf[256];
    for (unsigned long e = ERR_get_error(); e != 0; e = ERR_get_error()) {
        ERR_error_string_n(e, buf, sizeof(buf));
        if (!msg.empty()) msg += "; ";
        msg += buf;
    }
    return msg.empty() ? std::string("unknown OpenSSL error") : msg;
}

// Owning handles to the loaded OpenSSL providers.
//
// On OpenSSL 3.x the "default" provider hosts the modern digests (SHA-2/3,
// BLAKE2, ...) while legacy algorithms (RIPEMD-160, Whirlpool, ...) live in
// the "legacy" provider, which is NOT loaded by default on strict builds.
//
// IMPORTANT: a provider stays resident only while at least one OSSL_PROVIDER*
// handle to it is alive. Keep the returned ProviderHandles object alive for
// the whole lifetime the algorithms are needed (typically program lifetime).
// Discarding the handles immediately may unload the providers again.
struct ProviderHandles {
#if OPENSSL_VERSION_MAJOR >= 3
    OSSL_PROVIDER* default_provider = nullptr;
    OSSL_PROVIDER* legacy_provider = nullptr;
#endif

    // Release both providers. Safe to call on a default-constructed handle.
    void release() noexcept {
#if OPENSSL_VERSION_MAJOR >= 3
        if (default_provider) {
            OSSL_PROVIDER_unload(default_provider);
            default_provider = nullptr;
        }
        if (legacy_provider) {
            OSSL_PROVIDER_unload(legacy_provider);
            legacy_provider = nullptr;
        }
#endif
    }

    ~ProviderHandles() { release(); }
    ProviderHandles() = default;
    ProviderHandles(const ProviderHandles&) = delete;
    ProviderHandles& operator=(const ProviderHandles&) = delete;
    ProviderHandles(ProviderHandles&& other) noexcept {
#if OPENSSL_VERSION_MAJOR >= 3
        default_provider = other.default_provider;
        legacy_provider = other.legacy_provider;
        other.default_provider = nullptr;
        other.legacy_provider = nullptr;
#else
        (void)other;
#endif
    }
};

// Load the default and legacy providers (OpenSSL 3.x). Errors are ignored:
// if a provider is unavailable its algorithms simply stay unsupported. The
// returned handles keep the providers resident; retain them while needed.
inline ProviderHandles load_crypto_providers() noexcept {
    ProviderHandles h;
#if OPENSSL_VERSION_MAJOR >= 3
    h.default_provider = OSSL_PROVIDER_load(nullptr, "default");
    h.legacy_provider = OSSL_PROVIDER_load(nullptr, "legacy");
#endif
    return h;
}

}  // namespace cksumx
