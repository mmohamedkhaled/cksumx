// checksum_validator.cpp
//
// Implementation of the cksumx checksum library.

#include "cksumx/checksum_validator.hpp"

#include "crypto_util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace cksumx {

ChecksumValidator::ChecksumValidator(std::string algorithm, std::size_t chunk_size)
    : algorithm_(normalize(std::move(algorithm))),
      md_(EVP_get_digestbyname(algorithm_.c_str())),
      chunk_size_(chunk_size) {
    if (md_ == nullptr) {
        throw UnsupportedAlgorithmError("Unsupported algorithm '" + algorithm_ +
                                        "'. Use --list-algos to see available options.");
    }
    if (chunk_size_ == 0) {
        throw std::invalid_argument("chunk_size must be a positive integer");
    }
}

std::string ChecksumValidator::compute(const std::filesystem::path& file_path) const {
    return to_hex(digest_file(file_path));
}

std::string ChecksumValidator::compute_bytes(const std::string& data) const {
    return to_hex(digest_data(data));
}

bool ChecksumValidator::verify(const std::filesystem::path& file_path,
                               const std::string& expected_hex) const {
    return constant_time_equal(digest_file(file_path), hex_to_bytes(expected_hex));
}

bool ChecksumValidator::verify_bytes(const std::string& data, const std::string& expected_hex) const {
    return constant_time_equal(digest_data(data), hex_to_bytes(expected_hex));
}

std::vector<std::string> ChecksumValidator::available_algorithms() {
    static const std::vector<std::string> known = {
        "md5",        "sha1",        "sha224",      "sha256",      "sha384",
        "sha512",     "sha3-224",    "sha3-256",    "sha3-384",    "sha3-512",
        "blake2b512", "blake2s256",  "ripemd160"};
    std::vector<std::string> result;
    for (const auto& name : known) {
        if (EVP_get_digestbyname(name.c_str()) != nullptr) {
            result.push_back(name);
        }
    }
    return result;
}

std::string ChecksumValidator::normalize(std::string algorithm) {
    algorithm.erase(std::remove_if(algorithm.begin(), algorithm.end(),
                                   [](unsigned char c) { return std::isspace(c); }),
                    algorithm.end());
    std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    static const std::unordered_map<std::string, std::string> aliases = {
        {"sha-1", "sha1"},     {"sha-224", "sha224"}, {"sha-256", "sha256"},
        {"sha-384", "sha384"}, {"sha-512", "sha512"}, {"blake2b", "blake2b512"},
        {"blake2s", "blake2s256"}};
    auto it = aliases.find(algorithm);
    return it != aliases.end() ? it->second : algorithm;
}

std::vector<unsigned char> ChecksumValidator::digest_file(
    const std::filesystem::path& file_path) const {
    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec)) {
        throw FileReadError("No such file: " + file_path.string());
    }
    if (std::filesystem::is_directory(file_path, ec)) {
        throw FileReadError("Path is a directory, not a file: " + file_path.string());
    }

    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        throw FileReadError("Failed to open file: " + file_path.string());
    }

    EvpMdCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx || EVP_DigestInit_ex(ctx.get(), md_, nullptr) != 1) {
        throw ChecksumError("Failed to initialize digest: " + openssl_error());
    }

    std::vector<char> buffer(chunk_size_);
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        std::streamsize n = in.gcount();
        if (n > 0 &&
            EVP_DigestUpdate(ctx.get(), buffer.data(), static_cast<std::size_t>(n)) != 1) {
            throw ChecksumError("Failed to update digest: " + openssl_error());
        }
    }
    if (in.bad()) {
        throw FileReadError("I/O error while reading: " + file_path.string());
    }
    return finalize(ctx.get());
}

std::vector<unsigned char> ChecksumValidator::digest_data(const std::string& data) const {
    EvpMdCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx || EVP_DigestInit_ex(ctx.get(), md_, nullptr) != 1) {
        throw ChecksumError("Failed to initialize digest: " + openssl_error());
    }
    if (!data.empty() && EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1) {
        throw ChecksumError("Failed to update digest: " + openssl_error());
    }
    return finalize(ctx.get());
}

std::vector<unsigned char> ChecksumValidator::finalize(EVP_MD_CTX* ctx) const {
    std::vector<unsigned char> out(EVP_MAX_MD_SIZE);
    unsigned int out_len = 0;
    if (EVP_DigestFinal_ex(ctx, out.data(), &out_len) != 1) {
        throw ChecksumError("Failed to finalize digest: " + openssl_error());
    }
    out.resize(out_len);
    return out;
}

bool ChecksumValidator::constant_time_equal(const std::vector<unsigned char>& a,
                                            const std::vector<unsigned char>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

std::string ChecksumValidator::to_hex(const std::vector<unsigned char>& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char b : bytes) {
        oss << std::setw(2) << static_cast<unsigned long>(b);
    }
    return oss.str();
}

std::vector<unsigned char> ChecksumValidator::hex_to_bytes(std::string hex) {
    hex.erase(std::remove_if(hex.begin(), hex.end(),
                              [](unsigned char c) { return std::isspace(c) || c == ':'; }),
              hex.end());
    if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex.erase(0, 2);
    }
    if (hex.empty() || hex.size() % 2 != 0) {
        throw std::invalid_argument("Expected digest is not valid hex");
    }
    std::vector<unsigned char> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        char buf[3] = {hex[i], hex[i + 1], '\0'};
        char* end = nullptr;
        long value = std::strtol(buf, &end, 16);
        if (end == buf || *end != '\0') {
            throw std::invalid_argument("Expected digest contains non-hex characters");
        }
        out.push_back(static_cast<unsigned char>(value));
    }
    return out;
}

}  // namespace cksumx
