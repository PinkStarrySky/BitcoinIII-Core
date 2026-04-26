# Multiprocess BitcoinIII

_This document describes usage of the multiprocess feature. For design information, see the [design/multiprocess.md](design/multiprocess.md) file._

## Build Option

On Unix systems, the `-DWITH_MULTIPROCESS=ON` build option can be passed to build the supplemental `bitcoinIII-node` and `bitcoinIII-gui` multiprocess executables.

## Debugging

The `-debug=ipc` command line option can be used to see requests and responses between processes.

## Installation

The multiprocess feature requires [Cap'n Proto](https://capnproto.org/) and [libmultiprocess](https://github.com/bitcoinIII-core/libmultiprocess) as dependencies. A simple way to get started using it without installing these dependencies manually is to use the [depends system](../depends) with the `MULTIPROCESS=1` [dependency option](../depends#dependency-options) passed to make:

```
cd <BITCOINIII_SOURCE_DIRECTORY>
make -C depends NO_QT=1 MULTIPROCESS=1
# Set host platform to output of gcc -dumpmachine or clang -dumpmachine or check the depends/ directory for the generated subdirectory name
HOST_PLATFORM="x86_64-pc-linux-gnu"
cmake -B build --toolchain=depends/$HOST_PLATFORM/toolchain.cmake
cmake --build build
build/bin/bitcoinIII-node -regtest -printtoconsole -debug=ipc
BITCOINIIID=$(pwd)/build/bin/bitcoinIII-node build/test/functional/test_runner.py
```

The `cmake` build will pick up settings and library locations from the depends directory, so there is no need to pass `-DWITH_MULTIPROCESS=ON` as a separate flag when using the depends system (it's controlled by the `MULTIPROCESS=1` option).

Alternately, you can install [Cap'n Proto](https://capnproto.org/) and [libmultiprocess](https://github.com/bitcoinIII-core/libmultiprocess) packages on your system, and just run `cmake -B build -DWITH_MULTIPROCESS=ON` without using the depends system. The `cmake` build will be able to locate the installed packages via [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/). See [Installation](https://github.com/bitcoinIII-core/libmultiprocess/blob/master/doc/install.md) section of the libmultiprocess readme for install steps. See [build-unix.md](build-unix.md) and [build-osx.md](build-osx.md) for information about installing dependencies in general.

## Usage

`bitcoinIII-node` is a drop-in replacement for `bitcoinIIId`, and `bitcoinIII-gui` is a drop-in replacement for `bitcoinIII-qt`, and there are no differences in use or external behavior between the new and old executables. But internally after [#10102](https://github.com/bitcoinIII/bitcoinIII/pull/10102), `bitcoinIII-gui` will spawn a `bitcoinIII-node` process to run P2P and RPC code, communicating with it across a socket pair, and `bitcoinIII-node` will spawn `bitcoinIII-wallet` to run wallet code, also communicating over a socket pair. This will let node, wallet, and GUI code run in separate address spaces for better isolation, and allow future improvements like being able to start and stop components independently on different machines and environments.
[#19460](https://github.com/bitcoinIII/bitcoinIII/pull/19460) also adds a new `bitcoinIII-node` `-ipcbind` option and a `bitcoinIIId-wallet` `-ipcconnect` option to allow new wallet processes to connect to an existing node process.
And [#19461](https://github.com/bitcoinIII/bitcoinIII/pull/19461) adds a new `bitcoinIII-gui` `-ipcconnect` option to allow new GUI processes to connect to an existing node process.
