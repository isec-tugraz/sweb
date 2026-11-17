# Build and boot verification

[Build and boot](../.github/workflows/ci.yml) runs on pushes, pull requests, and
manual dispatches. It replaces the retired Travis configuration. Each architecture
has an independent Ubuntu 24.04 job using distribution-packaged GCC, CMake, and
QEMU (software emulation; KVM is not required). Jobs do not stop other matrix jobs
when one architecture fails. The token has read-only repository access.

| Architecture | CMake ARCH | Compiler | QEMU | Input / output |
| --- | --- | --- | --- | --- |
| x86_64 | x86/64 | native GCC | qemu-system-x86_64 | PS/2 keys via QMP / debug port |
| x86_32 | x86/32 | native GCC with -m32 | qemu-system-i386 | PS/2 keys via QMP / debug port |
| x86_32_pae | x86/32/pae | native GCC with -m32 | qemu-system-i386 | PS/2 keys via QMP / debug port |
| arm_rpi2 | arm/rpi2 | arm-none-eabi | qemu-system-arm, raspi2b | UART |
| armv8_rpi3 | armv8/rpi3 | aarch64-linux-gnu | qemu-system-aarch64, raspi3b | UART |

The Raspberry Pi 3 build sets `VIRTUALIZED_QEMU=1`. Native Raspberry Pi hardware
and the separate `armv8_rpi3_hardware` target are not covered by this workflow.

## What a pass means

The build compiles the kernel, userspace programs, and host utilities and creates
the disk image. The boot and shell check then requires all of the following:

1. QEMU starts and SWEB reaches `SWEB: />` within 60 seconds.
2. The harness sends `help` using the architecture's input device.
3. **New output after the command** contains `List all commands`, followed by a
   new shell prompt within 60 seconds. Help printed during startup cannot pass
   this check.

On x86, the harness presses Enter to leave the GRUB menu. ARM uses serial input,
not QEMU's emulated keyboard. QEMU runs with an ephemeral disk snapshot and is
terminated and reaped after the test, including failure and interruption paths.
There are no shared `/tmp/qemu` FIFOs or logs from earlier runs.

This verifies building, booting, and shell interaction. A complete kernel/filesystem
regression suite is outside its scope. Compiling userspace test programs does not mean they were run.
The harness regression job uses a fake QEMU process to check failure detection;
only the architecture matrix jobs execute real guest kernels.

## Run locally

On Ubuntu 24.04, install the common dependencies and the packages for your target:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends cmake make gcc g++ python3 qemu-utils
# x86 targets:
sudo apt-get install --no-install-recommends qemu-system-x86
# Raspberry Pi 2:
sudo apt-get install --no-install-recommends gcc-arm-none-eabi binutils-arm-none-eabi qemu-system-arm
# Raspberry Pi 3 (qemu-system-arm includes qemu-system-aarch64):
sudo apt-get install --no-install-recommends gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu qemu-system-arm
```

From the repository root, use a fresh, separate build directory per architecture:

```sh
mkdir ../sweb-build-x86_64
cd ../sweb-build-x86_64
../sweb/utils/travis_test.sh x86_64
```

`utils/travis_test.sh` retains its historical name, but no longer uses Travis or
the destructive architecture-switching make targets. Invoke it from its original
location; do not copy it into the build directory. It configures CMake with the
selected architecture and stops immediately if configuration or compilation
fails. A cache for another architecture is rejected to avoid retaining the wrong
compiler. `CMAKE_BUILD_PARALLEL_LEVEL` controls build parallelism (default: 2).

The workflow separates the same operations for clear reporting:

```sh
../sweb/utils/travis_test.sh --build-only x86_64
../sweb/utils/travis_test.sh --boot-only x86_64
```

For a slower host, run `python3 ../sweb/utils/qemu_boot.py x86_64 --timeout 120`
after building. The timeout applies separately to boot and command verification.

## Results and diagnosis

- **0 / PASS**: the requested operation completed. `--build-only` verifies only
  the build; it never claims that booting was checked.
- **77 / SKIP**: the emulator is missing or does not provide the requested board.
  The reason is printed and saved. A skip is not a successful boot test. All five
  targets are required in CI, so this exit status fails the matrix job visibly.
- **Other nonzero / FAIL**: configuration, compilation, emulator startup, boot,
  or shell verification failed. Invalid arguments/cache selection return 2;
  build failures preserve the failing command's exit status.

The Actions summary lists build and boot outcomes separately. Artifacts named
`ci-<architecture>` retain configure/build output, guest output (`guest.log`),
QEMU diagnostics (`qemu.log`), the emulator command (`command.json`), and the
boot verification result (`result.json`) for 14 days. Locally these live in `ci-logs/` under
the build directory. Missing boot results mean the configuration is unverified.

Run the harness regression tests from the repository root:

```sh
python3 -m unittest discover -s utils/tests -v
shellcheck utils/travis_test.sh
```
