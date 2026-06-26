
<cite>
**Referenced Files**
- [CMakeLists.txt](file://CMakeLists.txt)
- [configure.cmake](file://configure.cmake)
- [config.h.cmake](file://config.h.cmake)
- [cmake/](file://cmake/)
- [MYSQL_VERSION](file://MYSQL_VERSION)
</cite>

## Table of Contents
1. [Build System Overview](#build-system-overview)
2. [Platform Requirements](#platform-requirements)
3. [CMake Configuration](#cmake-configuration)
4. [Key Build Options](#key-build-options)
5. [Third-Party Dependencies](#third-party-dependencies)
6. [Build Targets](#build-targets)
7. [Installation Layout](#installation-layout)

## Build System Overview

MySQL Server uses **CMake** (minimum version 3.17.5) as its build system. The root [CMakeLists.txt](file://CMakeLists.txt) (~2935 lines) orchestrates the entire build, including platform detection, compiler configuration, feature detection, and target definitions. CMake modules in the `cmake/` directory handle specific subsystems: SSL detection (`ssl.cmake`), Boost integration (`boost.cmake`), Protobuf (`protobuf.cmake`), ICU (`icu.cmake`), Bison grammar generation (`bison.cmake`), and many more.

The build produces these primary artifacts:
- **mysqld** — The MySQL server daemon
- **mysqlrouter** — MySQL Router (middleware)
- **Client tools** — mysql, mysqldump, mysqladmin, mysqlbinlog, etc.
- **libmysqlclient** — Client shared/static library
- **Plugins** — Dynamic plugin shared libraries (.so/.dll)
- **Components** — Dynamic component shared libraries

**Sources** · [CMakeLists.txt:1-150](file://CMakeLists.txt#L1-L150)

## Platform Requirements

| Platform | Minimum Version | Compiler | CMake |
|----------|----------------|----------|-------|
| Linux (RHEL) | 6–10 | devtoolset-11 (RHEL7), gcc-toolset-14 (RHEL8/9), gcc-13 (SUSE 15) | ≥ 3.17.5 |
| macOS | 11+ | Apple Clang (Xcode) | ≥ 3.19 (Xcode) |
| Windows | 10 / Server 2016+ | Visual Studio 2019+ (MSVC) | ≥ 3.17.5 |
| Solaris | 11 | GCC 11 | ≥ 3.17.5 |

The build system auto-detects compiler toolchains via `cmake/os/` platform modules. On RHEL, it probes for devtoolset/gcc-toolset installations. Unsupported compilers require `-DFORCE_UNSUPPORTED_COMPILER=ON`.

**Sources** · [cmake/os/](file://cmake/os/)

## CMake Configuration

Standard build invocation:

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo .

# Build
cmake --build build -j$(nproc)

# Install
cmake --install build --prefix /usr/local/mysql
```

The default build type is `RelWithDebInfo` (optimized with debug info). Set `-DWITH_DEBUG=ON` for a Debug build with assertions and DBUG tracing enabled.

**Sources** · [CMakeLists.txt:119-132](file://CMakeLists.txt#L119-L132)

## Key Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | RelWithDebInfo | Build type: Debug, Release, RelWithDebInfo |
| `WITH_DEBUG` | OFF | Enable debug build with dbug/safemutex |
| `MAX_INDEXES` | 64U | Maximum indexes per table (max 255) |
| `WITH_SSL` | system | OpenSSL location or "system" |
| `WITH_ROUTER` | ON | Build MySQL Router |
| `WITH_NDB` | OFF | Build MySQL Cluster (NDB) support |
| `WITH_INNOBASE_STORAGE_ENGINE` | ON | Include InnoDB storage engine |
| `WITH_MYISAM_STORAGE_ENGINE` | ON | Include MyISAM storage engine |
| `WITH_ASAN` | OFF | Enable AddressSanitizer |
| `WITH_TSAN` | OFF | Enable ThreadSanitizer |
| `WITH_UBSAN` | OFF | Enable UndefinedBehaviorSanitizer |
| `WITH_BOOST` | (auto) | Path to Boost libraries |
| `WITH_PROTOBUF` | bundled | Protobuf source: "bundled" or "system" |
| `WITH_ICU` | bundled | ICU source: "bundled" or "system" |
| `WITH_CURL` | bundled | curl source for HTTP operations |
| `DOWNLOAD_BOOST` | OFF | Auto-download Boost if not found |
| `WITH_AUTH_*` | ON | Enable specific authentication plugins |

**Sources** · [CMakeLists.txt:128-150](file://CMakeLists.txt#L128-L150)

## Third-Party Dependencies

Bundled in `extra/` (built from source during cmake):

| Library | Version | Purpose |
|---------|---------|---------|
| Boost | Various | C++ utilities, geometry, program_options |
| Protobuf | Bundled | Serialization for internal protocols |
| RapidJSON | Bundled | JSON parsing and validation |
| ICU | Bundled | Unicode/internationalization |
| LZ4 | Bundled | Fast compression |
| Zstd | Bundled | High-ratio compression |
| zlib | Bundled | General-purpose compression |
| curl | Bundled | HTTP client for cloud/keyring integration |
| googletest | Bundled | C++ unit testing framework |
| libfido2 | Bundled | FIDO2/WebAuthn device authentication |
| libcbor | Bundled | CBOR encoding for FIDO2 |
| Abseil | Bundled | Google C++ common libraries |
| xxhash | Bundled | Fast non-cryptographic hashing |
| libedit | Bundled | Command-line editing (readline alternative) |
| gperftools | Bundled | tcmalloc/profiler |
| libbacktrace | Bundled | Stack trace symbolization |

External (system-provided):
- **OpenSSL** — TLS/SSL and cryptography (required)
- **Kerberos/GSSAPI** — Optional Kerberos authentication
- **LDAP** — Optional LDAP authentication
- **SASL** — Optional SASL authentication

**Sources** · [extra/](file://extra/) · [cmake/ssl.cmake](file://cmake/ssl.cmake) · [cmake/boost.cmake](file://cmake/boost.cmake)

## Build Targets

Major CMake targets:

| Target | Description |
|--------|-------------|
| `mysqld` | MySQL server daemon |
| `mysql` | Interactive SQL client |
| `mysqldump` | Logical backup utility |
| `mysqladmin` | Server administration tool |
| `mysqlbinlog` | Binary log utility |
| `mysqlrouter` | MySQL Router |
| `mysqltest` | MTR test client |
| `doxygen` | Generate Doxygen documentation |
| `unittest` | Build all Google Test unit tests |

**Sources** · [CMakeLists.txt](file://CMakeLists.txt)

## Installation Layout

The installation layout is controlled by `cmake/install_layout.cmake` and the `-DINSTALL_LAYOUT=<type>` option:

| Layout | Description |
|--------|-------------|
| STANDALONE | Self-contained installation (default for source builds) |
| RPM | RPM package layout |
| DEB | Debian package layout |
| SVR4 | Solaris SVR4 package layout |
| SGL | MySQL Cluster layout |

Key directories in STANDALONE layout: `bin/`, `lib/`, `include/`, `share/`, `support-files/`, `man/`, `docs/`.

**Sources** · [cmake/install_layout.cmake](file://cmake/install_layout.cmake)
