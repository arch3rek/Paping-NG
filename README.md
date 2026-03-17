# Paping-NG

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++](https://img.shields.io/badge/Language-C++17-00599C.svg)](https://en.cppreference.com/w/cpp/compiler_support)

**Paping-NG** – the modern successor to `paping`. Cross-platform TCP/UDP port ping tool rewritten from scratch in modern C++. Fast, lightweight, and open-source.

Replaces the unmaintained original [9minds/paping](https://github.com/9minds/paping) and legacy fixes like [arch3rek/Paping-fixed](https://github.com/arch3rek/Paping-fixed).

## Features

- **Familiar & Intuitive**: Works just like the original `paping` tool. The familiar command-line interface ensures zero learning curve for existing users.
- **Cross-platform**: Native support for Linux, macOS, Windows, and ARM architectures.
- **High Performance**: Rewritten from scratch in C++ for maximum efficiency.
- **Advanced Metrics**: Accurate RTT (min/avg/max), connection statistics, and timeouts.
- **Modern Networking**: Support for IPv4, IPv6, and standard TCP handshakes.
- **Drop-in Replacement**: Uses the exact same `paping` command name. You don't have to break your muscle memory or learn new tools – it simply works the way you expect.

## Installation

### Pre-built Binaries (Recommended)
Download the ready-to-use executables for Windows, Linux, and macOS from the [Releases](https://github.com/arch3rek/Paping-NG/releases) page.

**For Windows users:**
1. Download `paping-windows-amd64.exe`.
2. Rename the downloaded file to exactly `paping.exe`.
3. Drag and drop the file into your `C:\Windows\System32` directory (or `C:\Users\YourUsername`).
4. You can now open Command Prompt (cmd) or PowerShell and use the `paping` command from anywhere!

**For Linux / macOS users:**
1. Download the appropriate binary for your system (e.g., `paping-linux-amd64` or `paping-macos-arm64`).
2. Rename the file to `paping` and make it executable.
3. Move it to your local bin directory to use it globally.

```bash
# Example for Linux
mv paping-linux-amd64 paping
chmod +x paping
sudo mv paping /usr/local/bin/
```

### Build from source
If you prefer to compile it yourself, you will need `CMake` and a modern C++ compiler (GCC/Clang/MSVC):

```bash
git clone https://github.com/arch3rek/Paping-NG.git
cd Paping-NG
mkdir build && cd build
cmake ..
cmake --build .
```

## Usage

Paping-NG is designed to be highly intuitive. It mimics the behavior and arguments of the original tool, allowing you to seamlessly integrate it into your existing workflow.

```bash
# Basic TCP ping
paping example.com -p 80

# Specify amount of packets (-c)
paping 8.8.8.8 -p 53 -c 5

# See all options
paping --help
```

**Example Output:**
```text
Paping-NG v1.0 - Copyright (c) 2026 Oliver (arch3r.eu)

Connecting to google.com [142.250.120.139] on TCP port 80:

Connected to 142.250.120.139: time=80.70ms protocol=TCP port=80
Connected to 142.250.120.139: time=11.75ms protocol=TCP port=80
Connected to 142.250.120.139: time=12.40ms protocol=TCP port=80
Connected to 142.250.120.139: time=13.53ms protocol=TCP port=80

Connection statistics:
  Probed = 4, Connected = 4, Failed = 0 (0.00%)
Approximate connection times:
  Minimum = 11.75ms, Maximum = 80.70ms, Average = 29.60ms
```

## Why Paping-NG?

| Feature | Legacy `paping` | `Paping-NG` |
|---------|-----------------|-------------|
| **Status** | Abandoned (since ~2016) | Actively Maintained |
| **Codebase** | Old / Forked | Modern C++17 Rewrite |
| **Architecture** | 32-bit mostly | Full 64-bit / ARM |
| **License** | MIT | GPL-3.0 |

## Acknowledgments

Special thanks to [MatStef132](https://github.com/MatStef132) for their contributions and help with this project.

---
*Disclaimer: Paping-NG is an independent, from-scratch rewrite and is not affiliated with the original `9minds/paping` developers.*
