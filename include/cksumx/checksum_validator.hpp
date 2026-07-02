// checksum_validator.hpp
//
// Public API for cksumx: compute and verify cryptographic checksums of files
// or in-memory strings using OpenSSL's libcrypto.
//
// Files are streamed in fixed-size binary chunks so that arbitrarily large
// files (ISO images, database dumps, log archives) can be hashed in constant
// memory.
//
// Example:
//   cksumx::ChecksumValidator v("sha256");
//   std::string digest = v.compute("data.txt");
//   bool ok = v.verify("data.txt", "expected_hex_digest");

#pragma once

#include <cstddef>
#include <filesystem>
#include <openssl/evp.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace cksumx {

// Default bytes read per disk operation (64 KiB).
inline constexpr std::size_t kDefaultChunkSize = 64 * 1024;

// Default digest algorithm.
inline constexpr const char* kDefaultAlgorithm = "sha256";

// ---------------------------------------------------------------------------
// Exception hierarchy
// ---------------------------------------------------------------------------

/// Base class for every error raised by this module.
class ChecksumError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Raised when a requested hash algorithm is not provided by OpenSSL.
class UnsupportedAlgorithmError : public ChecksumError {
public:
    using ChecksumError::ChecksumError;
};

/// Raised when a file cannot be opened or read (missing, denied, ...).
class FileReadError : public ChecksumError {
public:
    using ChecksumError::ChecksumError;
};

// ---------------------------------------------------------------------------
// ChecksumValidator
// ---------------------------------------------------------------------------

/// Computes and verifies cryptographic digests of files or strings.
///
/// One instance is bound to a single algorithm but is reusable across many
/// inputs. Files are read in binary chunks so peak memory is independent of
/// input size.
class ChecksumValidator {
public:
    /// Construct a validator.
    ///
    /// \param algorithm  OpenSSL digest name (e.g. "sha256"). Hyphenated
    ///                   aliases such as "sha-256" are accepted.
    /// \param chunk_size Bytes read per disk operation. Must be positive.
    /// \throws UnsupportedAlgorithmError if \p algorithm is unavailable.
    /// \throws std::invalid_argument if \p chunk_size is not positive.
    explicit ChecksumValidator(std::string algorithm = kDefaultAlgorithm,
                               std::size_t chunk_size = kDefaultChunkSize);

    /// \return The canonical name of the configured digest.
    const std::string& algorithm() const noexcept { return algorithm_; }

    /// Compute the digest of a file, returned as lowercase hex.
    ///
    /// \param file_path Path to the file to hash.
    /// \throws FileReadError if the path is missing, is a directory, or is
    ///                       unreadable.
    std::string compute(const std::filesystem::path& file_path) const;

    /// Compute the digest of an in-memory byte string, returned as lowercase hex.
    ///
    /// \param data Raw bytes to hash.
    std::string compute_bytes(const std::string& data) const;

    /// Verify a file's digest against an expected value.
    ///
    /// Comparison uses CRYPTO_memcmp (constant time) to mitigate timing
    /// side-channels.
    ///
    /// \param file_path    Path to the file to verify.
    /// \param expected_hex Expected hexadecimal digest. Whitespace, "0x"
    ///                     prefixes, and ':' separators are ignored.
    /// \return true if the computed digest equals \p expected_hex.
    /// \throws FileReadError if the file cannot be read.
    /// \throws std::invalid_argument if \p expected_hex is not valid hex.
    bool verify(const std::filesystem::path& file_path, const std::string& expected_hex) const;

    /// Verify an in-memory string's digest against an expected value.
    ///
    /// \see verify(const std::filesystem::path&, const std::string&)
    bool verify_bytes(const std::string& data, const std::string& expected_hex) const;

    /// Enumerate the digest names provided by this OpenSSL build, drawn from a
    /// curated set of common algorithms.
    static std::vector<std::string> available_algorithms();

private:
    std::string algorithm_;
    const EVP_MD* md_;
    std::size_t chunk_size_;

    /// Lowercase, trim, and resolve friendly aliases (e.g. "SHA-256"->"sha256").
    static std::string normalize(std::string algorithm);

    /// Hash the full contents of a file into raw bytes.
    std::vector<unsigned char> digest_file(const std::filesystem::path& file_path) const;

    /// Hash an in-memory byte buffer into raw bytes.
    std::vector<unsigned char> digest_data(const std::string& data) const;

    /// Finish the digest computation and return raw bytes.
    std::vector<unsigned char> finalize(EVP_MD_CTX* ctx) const;

    /// Constant-time comparison of two equal-length byte buffers.
    static bool constant_time_equal(const std::vector<unsigned char>& a,
                                    const std::vector<unsigned char>& b);

    /// Render raw bytes as a lowercase hexadecimal string.
    static std::string to_hex(const std::vector<unsigned char>& bytes);

    /// Parse a hexadecimal string to raw bytes. Tolerates "0x", ':', and
    /// whitespace. Throws std::invalid_argument on malformed input.
    static std::vector<unsigned char> hex_to_bytes(std::string hex);
};

}  // namespace cksumx
