# Multimedia Library Management System in C

[![Language: C](https://img.shields.io/badge/Language-C99%20%7C%20C11-blue.svg?logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Build: CMake](https://img.shields.io/badge/Build-CMake%20%7C%20Makefile-064F8C?logo=cmake)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

> **High-Performance Multimedia Library System engineered in C.**  
> Features dynamic data structures, linked lists, binary file persistence, role-based access control, security lockout algorithms, and support for 9 distinct multimedia categories.

---

## 📌 Project Overview

This project implements an end-to-end multimedia catalog and borrowing management system built from scratch in C without third-party frameworks. It demonstrates fundamental computer science principles:
- Dynamic memory management (`malloc`, `free`, pointer manipulation).
- Multi-criteria linked list traversals and sorting.
- Robust binary file serialization (`.dat` binary storage).
- Multi-role authentication (Admin vs. Member) with lockout counters and session management.

---

## 🌟 Key Features

### 1. Multi-Media Catalog Support (9 Formats)
- **Reading**: Books, Magazines, Newspapers.
- **Watching**: Movies, TV Shows, Documentaries.
- **Listening**: Music, Podcasts, Audiobooks.

### 2. Multi-Role Security & Lockout Gate
- **Role Separation**: Administrator dashboard vs. Member self-service portal.
- **Security Lockout Engine**: Tracks failed attempts and enforces progressive lockouts (configurable regular vs. strict reset intervals).
- **Masked Password Input**: Terminal-level character masking for confidentiality.

### 3. Borrowing & Late Return Engine
- **Borrow Limits**: Up to 5 concurrent active loans per user.
- **Configurable Period**: Default 14-day loan window.
- **Penalty Tracking**: Automatic late-return flag and borrowing restriction enforcement upon exceeding allowed delays.

### 4. Search & Filter Algorithms
- Case-insensitive substring matching.
- Filter by media type, category, availability status, or author/creator.

---

## 🏗️ Repository Architecture

```
multimedia-library-management/
├── include/
│   └── library.h          # Header definitions, struct specifications & enums
├── src/
│   └── main.c             # Unified C implementation (2,100+ lines of robust C)
├── CMakeLists.txt         # Modern CMake build configuration
├── Makefile               # Standard GCC / Clang Makefile
├── .gitignore
└── README.md
```

---

## 🚀 Build & Installation

### With CMake (Recommended)
```bash
git clone https://github.com/IsraElAmri/multimedia-library-management.git
cd multimedia-library-management
mkdir build && cd build
cmake ..
cmake --build .
./library_system
```

### With GCC / Make
```bash
make
./library_system
```

### Windows (MSVC)
```cmd
cl /O2 /Fe:library_system.exe src\main.c
library_system.exe
```

---

## 👩‍💻 Author
**Isra El Amri**  
Mathematical Engineering & Modeling Student at ENSIT  
AI & Machine Learning Enthusiast  
Contact: [elamriisra3@gmail.com](mailto:elamriisra3@gmail.com)
