# cksumx - streaming cryptographic checksum utility.
cmake_minimum_required(VERSION 3.16)

project(cksumx
    VERSION 1.0.0
    DESCRIPTION "Streaming cryptographic checksum utility (SHA-1/2/3, BLAKE2, MD5, ...) built on OpenSSL"
    HOMEPAGE_URL "https://github.com/mmohamedkhaled/cksumx"
    LANGUAGES CXX)

# --- Language & compiler settings --------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug|Release|RelWithDebInfo)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo)
endif()

# Emit compile_commands.json for clangd / IDE integration.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CKSUMX_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# --- Dependencies -------------------------------------------------------------

find_package(OpenSSL REQUIRED)

# --- Library target -----------------------------------------------------------

add_library(cksumx STATIC
    src/checksum_validator.cpp)
add_library(cksumx::checksum ALIAS cksumx)

target_include_directories(cksumx
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(cksumx PUBLIC OpenSSL::Crypto)

# --- CLI executable -----------------------------------------------------------

add_executable(cksumx_cli src/cli.cpp)
set_target_properties(cksumx_cli PROPERTIES OUTPUT_NAME cksumx)

target_link_libraries(cksumx_cli PRIVATE cksumx)

# Inject the project version so the binary reports a single source of truth.
target_compile_definitions(cksumx_cli PRIVATE CKSUMX_VERSION="${PROJECT_VERSION}")

# --- Compiler warnings --------------------------------------------------------

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_library(cksumx_warnings INTERFACE)
    target_compile_options(cksumx_warnings INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wnon-virtual-dtor)
    if(CKSUMX_WARNINGS_AS_ERRORS)
        target_compile_options(cksumx_warnings INTERFACE -Werror)
    endif()
    target_link_libraries(cksumx PRIVATE cksumx_warnings)
    target_link_libraries(cksumx_cli PRIVATE cksumx_warnings)
endif()

# --- Installation -------------------------------------------------------------

include(GNUInstallDirs)

install(TARGETS cksumx
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(TARGETS cksumx_cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# --- Packaging metadata (pkg-config) -----------------------------------------

# Lets downstream `pkg-config --cflags --libs cksumx` consumers link easily.
configure_file(packaging/cksumx.pc.in cksumx.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cksumx.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# --- Testing -----------------------------------------------------------------

# Exposes the self-checking suite (OpenSSL CLI + sha256sum oracles) through
# CTest so contributors and CI can run `ctest` or `cmake --build build --target test`.
enable_testing()

find_program(BASH_EXECUTABLE bash REQUIRED)

add_test(
    NAME cksumx_selftest
    COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Point the harness at the freshly built binary instead of rebuilding itself.
set_tests_properties(cksumx_selftest PROPERTIES
    ENVIRONMENT "CKSUMX_BIN=$<TARGET_FILE:cksumx_cli>"
    LABELS "selftest"
    DESCRIPTION "Cross-checks digests against openssl and sha256sum oracles.")
