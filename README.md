![Windows](https://img.shields.io/badge/platform-Windows-blue)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![MSVC](https://img.shields.io/badge/compiler-MSVC-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Status](https://img.shields.io/badge/status-Beta-orange)

# NetWatch

**A focused TCP/UDP endpoint enumerator for Windows exploit development.**

NetWatch is a single-binary Windows tool that lists listening and established TCP/UDP connections alongside security-relevant process metadata. 

Built as a lightweight alternative to TCPView for the enumeration phase of vulnerability research. Instead of network traffic metrics, NetWatch surfaces what actually matters when identifying targets: `process architecture`, `integrity level`, `DEP/ASLR/CFG status`, `SafeSEH` and `executable paths` — everything you need before loading a binary into IDA.

## Quick Start
```cmd
netwatch.exe                 # all endpoints
netwatch.exe -l              # listening only (attack surface)
netwatch.exe -l --x86        # 32-bit listening services
netwatch.exe --filter Sync   # hunt for specific software
```


### Disclaimer
This code is currently in beta. Some areas may lack polish or optimal design patterns, as development has been fast-paced to reach an initial working version alongside my EXP-301 studies. Expect some refactoring and architectural improvements in upcoming releases.


#### WTL GUI
<img width="1422" height="845" alt="netwatch_sc" src="https://github.com/user-attachments/assets/3c3930a1-db50-4c2f-a803-7502392e5aa3" />
