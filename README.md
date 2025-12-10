![Windows](https://img.shields.io/badge/platform-Windows-blue)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![MSVC](https://img.shields.io/badge/compiler-MSVC-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Status](https://img.shields.io/badge/status-Alpha-orange)

# NetWatch

**A focused TCP/UDP endpoint enumerator for Windows exploit development.**

NetWatch is a single-binary Windows tool that lists listening and established TCP/UDP connections alongside security-relevant process metadata. 

Built as a lightweight alternative to TCPView for the enumeration phase of vulnerability research. Instead of network traffic metrics, NetWatch surfaces what actually matters when identifying targets: process architecture, integrity level, DEP/ASLR status, and executable paths — everything you need before loading a binary into IDA.

## Quick Start
```cmd
netwatch.exe                 # all endpoints
netwatch.exe -l              # listening only (attack surface)
netwatch.exe -l --x86        # 32-bit listening services
netwatch.exe --filter Sync   # hunt for specific software
```
