<img width="1500" height="500" alt="ChelikOS logo" src="https://github.com/user-attachments/assets/058259ca-dd4d-43b9-b49d-68a173dc8da8" /> <br>

![](https://img.shields.io/badge/version-0.0125-green)
![](https://img.shields.io/badge/License-GPL%203.0-yellow)

![](https://img.shields.io/badge/arch-x64-teal) <br>

ChelikOS is a Actual OS instead of a kernel with ramfs like cerulea

Differences
-
| Feature / Component | Cerulea | ChelikOS |
| :--- | :--- | :--- |
| **Project Type** | Kernel with ramfs | Actual Operating System |
| **Real Filesystem (FAT32)** | No | Yes |
| **Standard Library (`libc`)** | No | Yes  |
| **Architecture** | Minimal kernel | 64-bit Long Mode, Multiboot 2 |
| **Build System & Toolchain** | a build.txt file | Makefile |

Requirements
-
| System Specification | Requirement |
| :--- | :--- |
| **Minimum CPU** | AMD Athlon 64 |
| **Minimum RAM** | 32 MB |
| **External HDD** | 16 MB, Formatted in Fat32 |
| **VBE Version** | 2.0/3.0 |

Build Requirements
-
| Cross Compiler | Version |
| :--- | :--- |
| **x86_64-elf-gcc crosscompiler** | 15.30 |
| **x86_64-elf-ld** | 2.46.1 |
