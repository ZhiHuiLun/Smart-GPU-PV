# Smart GPU-PV Configuration Tool

> 🚀 **World's First GPU-PV Tool with Full Laptop Support**

A Windows desktop application for automatically configuring GPU Partitioning (GPU-PV) for Hyper-V virtual machines.

[中文文档](README.md) | [English](README_EN.md)

## ✨ Key Advantages

### 🎮 Laptop Support - Breakthrough Innovation!

**Smart-GPU-PV is the first GPU-PV configuration tool with complete laptop discrete GPU support!**

- ✅ **Supports Laptop NVIDIA GPUs** (RTX 4050 Laptop, RTX 4060 Laptop, RTX 4070 Laptop, etc.)
- ✅ **Intelligent GPU Name Matching**: Auto-detects "Laptop GPU", "Mobile" variants
- ✅ **Multi-tier Matching Strategy**: Solves laptop GPU driver recognition challenges
- ✅ **Verified on ThinkPad, Dell XPS, ASUS ROG, and more mainstream laptops**

**Comparison:** [Easy-GPU-PV](https://github.com/jamesstringer90/Easy-GPU-PV) officially states:
> "Laptops with NVIDIA GPUs are not supported at this time"

**Smart-GPU-PV breaks this limitation!** 🎉

### 🖥️ Graphical Interface vs Command Line

- ✅ Intuitive GUI operation, no need to memorize complex commands
- ✅ Real-time log display, operation process at a glance
- ✅ One-click configuration, suitable for all user levels

### 🔧 Enhanced Technical Features

- ✅ **WMI API Priority**: More reliable virtual machine management
- ✅ **Intelligent Driver Copying**: Automatically handles laptop GPU driver file specifics
- ✅ **Complete File Verification**: Ensures all required files are correctly copied
- ✅ **Detailed Error Messages**: Helps quickly locate and resolve issues

## Features

- ✅ Automatically detect all installed Hyper-V virtual machines
- ✅ Automatically identify partitionable GPUs and their details (name, VRAM, PCI path)
- ✅ Graphical interface, simple and intuitive operation
- ✅ One-click GPU-PV configuration, including:
  - Stop virtual machine
  - Add GPU partition adapter
  - Configure VRAM resources (customizable allocation size)
  - Configure Encode/Decode/Compute resources
  - Enable GuestControlledCacheTypes
  - Automatically find and copy GPU driver files to VM
- ✅ Real-time operation log display
- ✅ Complete error handling and user prompts

## System Requirements

- **Operating System**: Windows 10 20H1+ / Windows 11 (Pro/Enterprise/Education)
- **Virtualization**: Hyper-V feature enabled
- **Permissions**: Administrator privileges required
- **Hardware**: GPU-PV capable graphics card (typically NVIDIA/AMD discrete GPU)

## Build Instructions

### Prerequisites

1. **Visual Studio 2019/2022** (with components):
   - Desktop development with C++
   - Windows 10/11 SDK
   - CMake tools

2. **CMake 3.15+**

### Build Steps

#### Method 1: Using Visual Studio

1. Open project folder in Visual Studio
2. Visual Studio will auto-detect CMakeLists.txt and configure the project
3. Select `Build` > `Build Solution` (Ctrl+Shift+B)
4. Generated executable located in `build/bin/` directory

#### Method 2: Using CMake Command Line

```bash
# Create build directory
mkdir build
cd build

# Generate project files
cmake ..

# Build project
cmake --build . --config Release

# Executable located at build/bin/Release/Smart-GPU-PV.exe
```

## Usage Instructions

1. **Run as Administrator**
   - Right-click `Smart-GPU-PV.exe`
   - Select "Run as administrator"

2. **Configure GPU-PV**
   - Program automatically loads VM and GPU lists on startup
   - Select target virtual machine from dropdown
   - Select GPU to assign
   - Enter VRAM allocation size (in MB, recommended: 2048-8192)
   - Click "Configure GPU-PV" button
   - Wait for configuration to complete

3. **View Logs**
   - Operation logs display in real-time in the log area at the bottom
   - View execution status of each step

## PowerShell Command Comparison

This program automates the following manual PowerShell operations:

### Manual Operations (Multiple Steps Required)

```powershell
# 1. View partitionable GPUs
Get-VMHostPartitionableGpu

# 2. Stop virtual machine
Stop-VM -Name "VMName" -Force

# 3. Add GPU partition adapter
$gpuPath = "\\?\PCI#VEN_10DE&DEV_28A1..."
Add-VMGpuPartitionAdapter -VMName "VMName" -InstancePath $gpuPath

# 4. Configure GPU resources (Note: VRAM in bytes!)
$vmMaxVRAM = 4*1024*1024*1024  # 4GB
Set-VMGpuPartitionAdapter -VMName "VMName" -MinPartitionVRAM 1 -MaxPartitionVRAM $vmMaxVRAM -OptimalPartitionVRAM $vmMaxVRAM
Set-VMGpuPartitionAdapter -VMName "VMName" -MinPartitionEncode 1 -MaxPartitionEncode $vmMaxVRAM -OptimalPartitionEncode $vmMaxVRAM
Set-VMGpuPartitionAdapter -VMName "VMName" -MinPartitionDecode 1 -MaxPartitionDecode $vmMaxVRAM -OptimalPartitionDecode $vmMaxVRAM
Set-VMGpuPartitionAdapter -VMName "VMName" -MinPartitionCompute 1 -MaxPartitionCompute $vmMaxVRAM -OptimalPartitionCompute $vmMaxVRAM

# 5. Enable cache control
Set-VM -VMName "VMName" -GuestControlledCacheTypes $true

# 6. Find and copy driver files (Complex!)
$infFile = (Get-CimInstance Win32_VideoController | Where-Object {$_.Name -like "*RTX*"}).InfFilename
# ... Multiple steps to find driver folder and copy to VM
```

### Using This Program

Just in the GUI:
1. Select virtual machine
2. Select GPU
3. Enter VRAM size (MB)
4. Click "Configure" button

Everything done automatically! ✨

## Technical Details

### Project Structure

```
Smart-GPU-PV/
├── Smart-GPU-PV/
│   ├── Smart-GPU-PV.cpp            # Program entry point
│   ├── MainWindow.h/cpp            # Main window interface
│   ├── PowerShellExecutor.h/cpp    # PowerShell command executor
│   ├── VMManager.h/cpp             # Virtual machine management
│   ├── GPUManager.h/cpp            # GPU information management
│   ├── GPUPVConfigurator.h/cpp     # GPU-PV configuration logic
│   ├── WmiHelper.h/cpp             # WMI operation helper
│   ├── VhdHelper.h/cpp             # VHD operation helper
│   ├── Utils.h/cpp                 # Utility functions
│   └── resource files              # Icons and resources
├── CONTRIBUTING.md                 # Contribution guide
├── CHANGELOG.md                    # Change log
├── LICENSE                         # Apache 2.0 License
└── README.md / README_EN.md        # Documentation
```

### Core Technologies

- **GUI Framework**: Win32 API (native Windows dialogs)
- **PowerShell Interaction**: CreateProcess to create PowerShell processes and capture output
- **VM Management**: Hyper-V PowerShell Cmdlets + WMI API
- **GPU Detection**: WMI queries (Win32_VideoController, Msvm_PartitionableGpu)
- **Driver Copying**: VHD mounting, file system operations

## Notes

### ⚠️ VRAM Configuration Unit Correction

Common PowerShell command unit error:
```powershell
# ❌ Wrong: This is only 4MB, not 4GB!
$vmMaxVRAM = 4*1024*1024

# ✅ Correct: 4GB should be
$vmMaxVRAM = 4*1024*1024*1024
```

**This program has fixed this issue**: Users input in MB, program auto-converts to bytes.

### Other Notes

1. Virtual machine must be stopped to configure GPU-PV
2. Driver file copying requires VM disk to be accessible
3. After first configuration, GPU drivers must be installed in the VM
4. Some laptop discrete GPUs may not support GPU-PV

## 🙏 Acknowledgements and Comparison

This project's implementation references the methodology and best practices of [Easy-GPU-PV](https://github.com/jamesstringer90/Easy-GPU-PV). Easy-GPU-PV is an excellent PowerShell script tool that provides convenient solutions for desktop GPU-PV configuration.

### Smart-GPU-PV's Innovation and Improvements

#### 🌟 Core Breakthrough: Laptop Computer Support

Easy-GPU-PV official documentation clearly states:
> "Laptops with NVIDIA GPUs are not supported at this time, but Intel integrated GPUs work on laptops."

**Smart-GPU-PV breaks this limitation!** Through the following technical innovations to achieve laptop discrete GPU support:

1. **Intelligent GPU Name Matching Algorithm**
   - Supports laptop GPU naming like "NVIDIA GeForce RTX 4050 Laptop GPU"
   - Automatically handles suffixes like "Laptop", "Mobile", "Max-Q"
   - 5-tier matching strategy ensures correct driver file identification

2. **Enhanced Driver File Handling**
   - Special handling of laptop GPU driver paths
   - OEM customized driver compatibility handling
   - Complete file verification mechanism

3. **Actual Test Verification**
   - ThinkPad T14p Gen2 (RTX 4050 Laptop) ✅
   - Dell XPS series ✅
   - ASUS ROG series ✅
   - More models continuously being tested...

#### Other Major Improvements

- ✅ Complete reimplementation in C++/Win32
- ✅ Provides graphical user interface (vs command line)
- ✅ Added WMI API support and error handling
- ✅ Added driver file verification and auto-repair features
- ✅ Detailed Chinese and English documentation and error messages

Special thanks to [jamesstringer90](https://github.com/jamesstringer90) and all Easy-GPU-PV contributors for their contributions to popularizing GPU-PV technology.

## 📄 License

This project is licensed under the [Apache License 2.0](LICENSE). See LICENSE file for details.

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for details.

**We especially welcome:**
- 🎮 Laptop model testing reports (most valuable!)
- 🔧 AMD laptop GPU support
- 🌐 Translation to more languages
- 📝 Documentation improvements

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for version history.

---

**Let's make GPU-PV work on every machine! 🚀**
