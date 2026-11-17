# Multimedia Library Management System

[![CI](https://github.com/IsraElAmri/multimedia-library-management/actions/workflows/ci.yml/badge.svg)](https://github.com/IsraElAmri/multimedia-library-management/actions/workflows/ci.yml)
[![C Standard](https://img.shields.io/badge/C-C99-00599C?logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C99)
[![Build System](https://img.shields.io/badge/Build-CMake%20%7C%20Make-064F8C?logo=cmake&logoColor=white)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A terminal-based multimedia catalog and borrowing management system implemented in C99 using dynamic singly linked lists and binary file persistence. Tested on Windows and Linux (CI).

## Overview

Developed as an academic systems project at ENSIT to demonstrate manual memory management, dynamic data structures, and state persistence without external runtime dependencies. The application coordinates media tracking across 9 distinct categories and implements basic role-based access control with security lockout counters.

## Features and Verified Specifications

- **Media Catalog**: Supports 9 formats split across reading (books, magazines, newspapers), visual (movies, TV shows, documentaries), and audio (music, podcasts, audiobooks).
- **Dynamic Data Structures**: Fully manual memory management using singly linked lists for in-memory catalog navigation and dynamic client borrowing arrays.
- **Data Persistence**: Direct binary serialization (`.bin` files) for catalog state, active loans, and client records across restarts.
- **Security & Access Control**: Basic dual-role authentication (Admin and Client) with hashed passwords, session lockout after 3 consecutive failed attempts, and permanent lockout after 9 failures.

## Approach

- **Portability**: Pure C99 with cross-platform terminal abstractions (`system("cls")` on Windows, ANSI escape sequences on POSIX; `<conio.h>` on Windows, `<termios.h>` / `<unistd.h>` on POSIX).
- **Zero Dependencies**: Self-contained single-translation-unit architecture requiring only a standard C library and compiler.

## Quick Start

Tested on Windows and Linux (CI).

### Build with GCC / Clang
```bash
gcc -std=c99 -Wall -Wextra -O2 src/main.c -o library_system
./library_system
```

### Build with CMake
```bash
mkdir build && cd build
cmake ..
cmake --build .
./library_system
```

### Windows (MSVC)
```cmd
cl /O2 /Fe:library_system.exe src\main.c
library_system.exe
```

## Project Structure

```
multimedia-library-management/
├── .github/
│   └── workflows/
│       └── ci.yml
├── CMakeLists.txt
├── Makefile
├── LICENSE
├── README.md
└── src/
    └── main.c
```

## Limitations and Future Improvements

- **Binary Serialization**: Data is stored using raw `fread`/`fwrite` on in-memory struct layouts; binary data files are not guaranteed to be portable across different machine architectures or compiler struct paddings.
- **Credential Storage**: Default demo administrator credentials (`admin` / `password123`) are hardcoded for evaluation.
- **Single-File Implementation**: The core logic resides in a single source file (`src/main.c`), typical for early course assignments but intended for modular refactoring.

## Author

- **Isra El Amri** - [GitHub Profile](https://github.com/IsraElAmri)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
