# zephyrDas

Zephyr firmware that AOTs [daslang](https://github.com/GaijinEntertainment/daScript) to C++ and links **libDaScriptNano** — a runtime with no compiler on the MCU.

`west build` generates Zephyr bindings, compiles a host daslang plugin, AOTs `main.das` with [`cmake/aot_driver.das`](cmake/aot_driver.das), and links the result against nano.

Tested on **nucleo_f446re** (STM32F4). Other chips need [`bindings/generator/include_dirs.txt`](bindings/generator/include_dirs.txt) retuned to that SoC.

## Prerequisites

- An installed **daslang SDK** (`daslang` on `PATH`, `find_package(DAS)` — typically `/usr/local`)
- Host C/C++ compiler (used only for the daslang plugin, not the firmware)
- Homebrew LLVM on macOS if you regenerate bindings (`CLANG_RESOURCE_DIR`, or `/opt/homebrew/opt/llvm/lib/clang`)

## Setup

Install west in a virtual environment in this directory, or optionally globally:

```sh
pip3 install -U west
```

On Linux you can instead use:

```sh
pip3 install --user -U west
```

Then initialize and update west (this may take a while):

```sh
west init
west update
```

Install the ARM Zephyr toolchain:

```sh
west sdk install --toolchain=arm-zephyr-eabi
```

## Build and flash

From the workspace root:

```sh
west build -p -b nucleo_f446re samples/hello_world
west flash
```

The sample prints `hello` on the UART console and configures `led0`. Bindings and AOT run as part of this build; you do not need a separate bind step first.

To add another sample, copy `samples/hello_world` and keep:

```cmake
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/das_firmware.cmake")
das_zephyr_app(src/main.das src/main.cpp)
```

Set `CONFIG_REQUIRES_FULL_LIBCPP=y` in `prj.conf` (nano uses the STL).

## Generate bindings (optional)

West already runs the binder after Zephyr emits `syscall_list.h`. To regenerate by hand (only after a west build has produced those headers):

```sh
cd bindings/generator
daslang bind_zephyr.das
cmake -S . -B build && cmake --build build
```

Or, from an existing generator build directory:

```sh
cmake --build bindings/generator/build -t regen-binds
```

## Renode simulation

1. Install [Renode](https://github.com/renode/renode).
2. Build the sample (see above) so `build/zephyr/zephyr.elf` exists.
3. Run:

   ```sh
   cd renode
   renode main.resc
   ```

   If you built Renode from source, use `renode --ui main.resc`. USART2 is attached as an analyzer; you should see `hello`.
