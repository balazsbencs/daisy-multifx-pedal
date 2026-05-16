---
title: Building & Flashing
description: Toolchain setup, build commands, DFU flashing, and the Daisy bootloader
---

## Prerequisites

- `arm-none-eabi-gcc` toolchain (10.3+)
- `make`
- A populated `third_party/libDaisy` and `third_party/DaisySP`
  (both ship as git submodules)

Initialize submodules and build the libraries once:

```sh
git submodule update --init --recursive
make -C third_party/libDaisy
make -C third_party/DaisySP
```

## Build

```sh
make            # produces build/multi-fx.{elf,bin,hex}
```

Or with parallel jobs for faster incremental builds:

```sh
make -j4
```

Output is built with `-Os -flto`. The binary targets the QSPI flash
via the Daisy bootloader (`APP_TYPE = BOOT_QSPI`).

The Makefile pads the `.bin` to ≥131073 bytes so the last byte falls in a
DFU-writeable QSPI sector — without the pad, `dfu-util` aborts with
"Last page … is not writeable".

## Flash

The pedal expects the [Daisy Bootloader](https://github.com/electro-smith/DaisyBootloader)
to already be present (so `APP_TYPE=BOOT_QSPI` works). To flash the bootloader
once, follow Electrosmith's instructions.

To flash the application over USB DFU (Daisy in bootloader mode — hold BOOT,
press RESET):

```sh
make program-dfu
```

Internally this runs:

```sh
dfu-util -a 0 -s 0x90040000:leave -D build/multi-fx.bin -d ,0483:df11
```

## Memory layout

| Region    | Contents                                         |
|-----------|--------------------------------------------------|
| QSPI flash | Application code + rodata (via Daisy bootloader) |
| DTCMRAM   | Stack, fast data, modulation-FX delay lines      |
| SDRAM     | Long delay lines (`DSY_SDRAM_BSS`), display frame buffer |

No heap allocation occurs anywhere in the firmware. All buffers are statically
allocated at file scope so their placement is fully controlled by the linker.

## Build constants

Key compile-time knobs live in `src/config/`:

| Symbol | Default | Meaning |
|--------|---------|---------|
| `AUDIO_BLOCK_SIZE` | 48 | Samples per audio callback (1 ms at 48 kHz) |
| `NUM_PRESET_SLOTS` | 8 | Persistent preset slots in QSPI |
| `APP_TYPE` | `BOOT_QSPI` | Daisy linker script selection |
