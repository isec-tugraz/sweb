# License

All rights reserved. Permission is not granted to modify this software or create any derivative works based on it.
Any alteration, adaptation, transformation, or derived version of this software is strictly prohibited without explicit written permission from the copyright holder.

# sweb
[![Build and boot](https://github.com/isec-tugraz/sweb/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/isec-tugraz/sweb/actions/workflows/ci.yml)

SWEB Educational OS

Please have a look at https://www.isec.tugraz.at/os

## Continuous integration

GitHub Actions builds and tests all five supported CI architectures on Ubuntu 24.04.
Each QEMU boot and shell check must reach the shell, execute `help`, and return to the prompt.
See [CI coverage and local reproduction](utils/CI.md) for dependencies, commands,
exit statuses, and the limits of these checks.
