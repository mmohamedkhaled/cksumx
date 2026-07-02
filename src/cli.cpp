// cli.cpp
//
// Command-line front-end for the cksumx checksum library.
//
// Build (standalone, outside CMake):
//   g++ -std=c++17 -O2 -Wall -Wextra -o cksumx
//       src/cli.cpp src/checksum_validator.cpp -lcrypto -Iinclude

#include "cksumx/checksum_validator.hpp"

#include "crypto_util.hpp"

#include <cctype>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

#ifndef CKSUMX_VERSION
#define CKSUMX_VERSION "0.0.0"
#endif

namespace {

const char* const kVersion = CKSUMX_VERSION;

struct CliArgs {
    std::string file;
    std::string str;
    std::string algo = cksumx::kDefaultAlgorithm;
    std::string verify;
    std::size_t chunk_size = cksumx::kDefaultChunkSize;
    bool list_algos = false;
    bool help = false;
    bool version = false;
};

std::string base_name(const char* arg0) {
    std::string s = arg0;
    auto pos = s.find_last_of("/\\");
    return pos == std::string::npos ? s : s.substr(pos + 1);
}

void print_help(const std::string& prog) {
    std::cout << "cksumx " << kVersion
              << " - compute and verify cryptographic checksums\n"
                 "\n"
                 "Usage: "
              << prog
              << " [OPTIONS]\n"
                 "\n"
                 "Options:\n"
                 "  -f, --file <path>        File to hash or verify.\n"
                 "  -a, --algo <name>        Hash algorithm (default: "
              << cksumx::kDefaultAlgorithm
              << ").\n"
                 "  -v, --verify <hex>       Verify file/string against this digest.\n"
                 "  -s, --string <text>      Hash a literal string instead of a file.\n"
                 "      --chunk-size <n>     Bytes read per disk operation (default: "
              << cksumx::kDefaultChunkSize
              << "). Suffixes K/M/G allowed.\n"
                 "      --list-algos         List available algorithms and exit.\n"
                 "      --version            Show version and exit.\n"
                 "  -h, --help               Show this help and exit.\n"
                 "\n"
                 "Exit codes: 0 success/verified, 1 verification failed, 2 usage/runtime error.\n";
}

// Parse a positive size with optional K/M/G suffix. Rejects overflow.
std::size_t parse_size(const std::string& raw) {
    if (raw.empty()) {
        throw std::invalid_argument("--chunk-size requires a value");
    }
    std::size_t digits = 0;
    while (digits < raw.size() && std::isdigit(static_cast<unsigned char>(raw[digits]))) {
        ++digits;
    }
    if (digits == 0) {
        throw std::invalid_argument("invalid chunk-size: " + raw);
    }
    long long value = 0;
    try {
        value = std::stoll(raw.substr(0, digits));
    } catch (...) {
        throw std::invalid_argument("invalid chunk-size: " + raw);
    }
    if (value <= 0) {
        throw std::invalid_argument("chunk-size must be positive");
    }
    if (digits < raw.size()) {
        unsigned char suffix = static_cast<unsigned char>(
            std::tolower(static_cast<unsigned char>(raw[digits])));
        constexpr long long k = 1024LL;
        long long multiplier = 1;
        switch (suffix) {
            case 'k': multiplier = k; break;
            case 'm': multiplier = k * k; break;
            case 'g': multiplier = k * k * k; break;
            default: throw std::invalid_argument("invalid chunk-size suffix: " + raw);
        }
        if (digits + 1 != raw.size()) {
            throw std::invalid_argument("invalid chunk-size: " + raw);
        }
        // Guard against overflow when applying the suffix.
        if (value > std::numeric_limits<long long>::max() / multiplier) {
            throw std::invalid_argument("chunk-size out of range: " + raw);
        }
        value *= multiplier;
    }
    if (static_cast<unsigned long long>(value) >
        std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("chunk-size out of range: " + raw);
    }
    return static_cast<std::size_t>(value);
}

CliArgs parse_args(int argc, char** argv) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto eq = arg.find('=');
        std::string name = (eq == std::string::npos) ? arg : arg.substr(0, eq);
        std::string embedded = (eq == std::string::npos) ? "" : arg.substr(eq + 1);

        auto take = [&](const std::string& flag) -> std::string {
            if (eq != std::string::npos) return embedded;
            if (i + 1 >= argc) {
                throw std::invalid_argument("missing value for " + flag);
            }
            return std::string(argv[++i]);
        };

        if (arg == "-h" || arg == "--help") {
            args.help = true;
        } else if (arg == "--version") {
            args.version = true;
        } else if (arg == "--list-algos") {
            args.list_algos = true;
        } else if (name == "-f" || name == "--file") {
            args.file = take(name);
        } else if (name == "-a" || name == "--algo") {
            args.algo = take(name);
        } else if (name == "-v" || name == "--verify") {
            args.verify = take(name);
        } else if (name == "-s" || name == "--string") {
            args.str = take(name);
        } else if (name == "--chunk-size") {
            args.chunk_size = parse_size(take(name));
        } else {
            throw std::invalid_argument("unknown option: " + arg);
        }
    }
    return args;
}

}  // namespace

int main(int argc, char** argv) {
    // Load OpenSSL providers and keep the handles alive for program lifetime.
    // On OpenSSL 3.x this makes legacy digests (RIPEMD-160, Whirlpool) available
    // even on strict builds where the legacy provider is not enabled by default.
    auto providers = cksumx::load_crypto_providers();
    (void)providers;
    OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_DIGESTS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);

    const std::string prog = base_name(argv[0]);

    CliArgs args;
    try {
        args = parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        std::cerr << "Run '" << prog << " --help' for usage.\n";
        return 2;
    }

    if (args.help) {
        print_help(prog);
        return 0;
    }
    if (args.version) {
        std::cout << "cksumx " << kVersion << "\n";
        return 0;
    }
    if (args.list_algos) {
        std::cout << "Available algorithms:\n";
        for (const auto& name : cksumx::ChecksumValidator::available_algorithms()) {
            std::cout << "  " << name << "\n";
        }
        return 0;
    }

    if (args.file.empty() && args.str.empty()) {
        std::cerr << "error: one of --file/--string or --list-algos is required\n";
        std::cerr << "Run '" << prog << " --help' for usage.\n";
        return 2;
    }
    if (!args.file.empty() && !args.str.empty()) {
        std::cerr << "error: specify either --file or --string, not both\n";
        return 2;
    }

    cksumx::ChecksumValidator validator;
    try {
        validator = cksumx::ChecksumValidator(args.algo, args.chunk_size);
    } catch (const cksumx::UnsupportedAlgorithmError& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }

    try {
        std::string label = args.str.empty() ? args.file : std::string("<string>");
        std::string digest = args.str.empty() ? validator.compute(args.file)
                                              : validator.compute_bytes(args.str);

        if (!args.verify.empty()) {
            std::cout << validator.algorithm() << " (" << label << ") = " << digest << "\n";
            bool ok = args.str.empty() ? validator.verify(args.file, args.verify)
                                       : validator.verify_bytes(args.str, args.verify);
            if (ok) {
                std::cout << "OK - checksum matches\n";
                return 0;
            }
            std::cerr << "FAILED - checksum does not match\n";
            std::cerr << "  expected: " << args.verify << "\n";
            std::cerr << "  actual:   " << digest << "\n";
            return 1;
        }

        std::cout << digest << "\n";
        return 0;
    } catch (const cksumx::FileReadError& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    } catch (const cksumx::ChecksumError& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
