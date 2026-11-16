# Multimedia Library Management System

[![C Standard](https://img.shields.io/badge/C-C99-00599C?logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C99)
[![Build System](https://img.shields.io/badge/Build-CMake%20%7C%20Make-064F8C?logo=cmake&logoColor=white)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A terminal-based multimedia catalog and borrowing management system implemented in C99 using dynamic singly linked lists and binary file persistence.

## Overview

Developed as an academic systems project at ENSIT to demonstrate manual memory management, dynamic data structures, and state persistence without external runtime dependencies. The application coordinates media tracking across 9 distinct categories and implements basic role-based access control with security lockout counters.

## Features and Verified Specifications

- **Media Catalog**: Supports 9 formats split across reading (books, magazines, newspapers), visual (movies, TV shows, documentaries), and audio (music, podcasts, audiobooks).
- **In-Memory Data Structures**: Dynamic linked lists allocated on the heap (`malloc`/`free`) for catalog items and active loans.
- **Persistence Layer**: Custom binary file storage for media items (`library.dat`), registered borrowers (`clients.dat`), credentials (`admin_credentials.dat`), and lockout state (`security.dat`).
- **Access Control & Lockout**: Distinct administrator and member interfaces. Progressive lockout triggers after 3 consecutive failed login attempts (temporary reset timer: 60 minutes default or 24 hours strict mode) and permanent lock after 9 failed attempts.
- **Borrowing Rules**: Enforces a limit of 5 concurrent loans per member with a standard 14-day loan window and late-return flagging.

## Approach

The system uses an in-memory linked list during interactive execution to keep traversal and lookup simple without a database engine. On startup, binary state is read from `.dat` files into heap structures, and updates are serialized back to disk. Password masking is handled at the console level, with input validation routines guarding against buffer overflow and invalid formats.

## Quick Start

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
.
├── .gitignore
├── CMakeLists.txt
├── LICENSE
├── Makefile
├── README.md
└── src/
    └── main.c
```

## Limitations and Future Improvements

- **Security**: Passwords and user records are stored in unencrypted binary files without cryptographic hashing (e.g., Argon2 or bcrypt). Default demo credentials are `admin/password123`.
- **Concurrency**: File I/O is single-process and synchronous; concurrent access by multiple users is not supported.
- **Architecture**: The entire implementation resides in a single 2,198-line source file (`src/main.c`), which can be modularized into discrete translation units (`catalog.c`, `auth.c`, `storage.c`).

## Author

**Isra El Amri**  
Mathematical Engineering & Modeling Student, ENSIT  
Email: [elamriisra3@gmail.com](mailto:elamriisra3@gmail.com) · Location: Sousse, Tunisia

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
