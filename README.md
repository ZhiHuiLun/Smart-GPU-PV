# Smart GPU-PV 配置工具

> 🚀 **首个支持笔记本电脑GPU的GPU-PV配置工具**

一个用于为Hyper-V虚拟机自动配置GPU分区（GPU-PV）的Windows桌面应用程序。

## ✨ 核心优势

### 🎮 支持笔记本电脑 - 突破性创新！

**Smart-GPU-PV是首个完整支持笔记本电脑独立显卡的GPU-PV工具！**

- ✅ **支持笔记本NVIDIA GPU**（如RTX 4050 Laptop、RTX 4060 Laptop、RTX 4070 Laptop等）
- ✅ **智能GPU名称匹配**：自动识别"Laptop GPU"、"Mobile"等变体
- ✅ **多级匹配策略**：解决笔记本GPU驱动识别难题
- ✅ **ThinkPad、Dell XPS、华硕ROG等主流笔记本验证通过**

**对比：** [Easy-GPU-PV](https://github.com/jamesstringer90/Easy-GPU-PV) 官方明确表示：
> "Laptops with NVIDIA GPUs are not supported at this time"

**Smart-GPU-PV突破了这个限制！** 🎉

### 🖥️ 图形化界面 vs 命令行

- ✅ 直观的GUI操作界面，无需记忆复杂命令
- ✅ 实时日志显示，操作过程一目了然
- ✅ 一键配置，适合所有用户水平

### 🔧 增强的技术特性

- ✅ **WMI API优先**：更可靠的虚拟机管理
- ✅ **智能驱动复制**：自动处理笔记本GPU驱动文件特殊性
- ✅ **完整文件验证**：确保所有必需文件正确复制
- ✅ **详细错误提示**：帮助快速定位和解决问题

## 功能特性

- ✅ 自动检测所有已安装的Hyper-V虚拟机
- ✅ 自动识别支持分区的GPU及其详细信息（名称、显存、PCI路径）
- ✅ 图形化界面，操作简单直观
- ✅ 一键配置GPU-PV，包括：
  - 停止虚拟机
  - 添加GPU分区适配器
  - 配置VRAM资源（可自定义分配大小）
  - 配置Encode/Decode/Compute资源
  - 启用GuestControlledCacheTypes
  - 自动查找并复制GPU驱动文件到虚拟机
- ✅ 实时显示操作日志
- ✅ 完整的错误处理和用户提示

## 系统要求

- **操作系统**: Windows 10 20H1+ / Windows 11 (专业版/企业版/教育版)
- **虚拟化**: 启用Hyper-V功能
- **权限**: 需要管理员权限运行
- **硬件**: 支持GPU-PV的显卡（通常为NVIDIA/AMD独立显卡）

## 构建说明

### 前置要求

1. **Visual Studio 2019/2022**（包含以下组件）：
   - 使用C++的桌面开发
   - Windows 10/11 SDK
   - CMake工具

2. **CMake 3.15+**

### 构建步骤

#### 方法1：使用Visual Studio

1. 在Visual Studio中打开项目文件夹
2. Visual Studio会自动检测CMakeLists.txt并配置项目
3. 选择 `生成` > `生成解决方案` (Ctrl+Shift+B)
4. 生成的可执行文件位于 `build/bin/` 目录

#### 方法2：使用CMake命令行

```bash
# 创建构建目录
mkdir build
cd build

# 生成项目文件
cmake ..

# 构建项目
cmake --build . --config Release

# 可执行文件位于 build/bin/Release/Smart-GPU-PV.exe
```

## 使用说明

1. **以管理员身份运行程序**
   - 右键点击 `Smart-GPU-PV.exe`
   - 选择"以管理员身份运行"

2. **配置GPU-PV**
   - 程序启动后会自动加载虚拟机和GPU列表
   - 在下拉框中选择要配置的虚拟机
   - 选择要分配的GPU
   - 输入要分配的显存大小（MB为单位，建议值：2048-8192）
   - 点击"配置 GPU-PV"按钮
   - 等待配置完成

3. **查看日志**
   - 操作日志会实时显示在界面下方的日志区域
   - 可以查看每个步骤的执行情况

## PowerShell命令对比

本程序自动化了以下手动PowerShell操作：

### 手动操作（需要多个步骤）

```powershell
# 1. 查看支持分区的GPU
Get-VMHostPartitionableGpu

# 2. 停止虚拟机
Stop-VM -Name "虚拟机名" -Force

# 3. 添加GPU分区适配器
$gpuPath = "\\?\PCI#VEN_10DE&DEV_28A1..."
Add-VMGpuPartitionAdapter -VMName "虚拟机名" -InstancePath $gpuPath

# 4. 配置GPU资源（注意：显存单位为字节！）
$vmMaxVRAM = 4*1024*1024*1024  # 4GB
Set-VMGpuPartitionAdapter -VMName "虚拟机名" -MinPartitionVRAM 1 -MaxPartitionVRAM $vmMaxVRAM -OptimalPartitionVRAM $vmMaxVRAM
Set-VMGpuPartitionAdapter -VMName "虚拟机名" -MinPartitionEncode 1 -MaxPartitionEncode $vmMaxVRAM -OptimalPartitionEncode $vmMaxVRAM
Set-VMGpuPartitionAdapter -VMName "虚拟机名" -MinPartitionDecode 1 -MaxPartitionDecode $vmMaxVRAM -OptimalPartitionDecode $vmMaxVRAM
Set-VMGpuPartitionAdapter -VMName "虚拟机名" -MinPartitionCompute 1 -MaxPartitionCompute $vmMaxVRAM -OptimalPartitionCompute $vmMaxVRAM

# 5. 启用缓存控制
Set-VM -VMName "虚拟机名" -GuestControlledCacheTypes $true

# 6. 查找并复制驱动文件（复杂！）
$infFile = (Get-CimInstance Win32_VideoController | Where-Object {$_.Name -like "*RTX*"}).InfFilename
# ... 多个步骤查找驱动文件夹并复制到虚拟机
```

### 使用本程序

只需在GUI中：
1. 选择虚拟机
2. 选择GPU
3. 输入显存大小（MB）
4. 点击"配置"按钮

一切自动完成！✨

## 技术细节

### 项目结构

```
Smart-GPU-PV/
├── src/
│   ├── main.cpp                    # 程序入口
│   ├── MainWindow.h/cpp            # 主窗口界面
│   ├── PowerShellExecutor.h/cpp    # PowerShell命令执行器
│   ├── VMManager.h/cpp             # 虚拟机管理
│   ├── GPUManager.h/cpp            # GPU信息管理
│   ├── GPUPVConfigurator.h/cpp    # GPU-PV配置逻辑
│   ├── Utils.h/cpp                 # 工具函数
│   └── resource.h                  # 资源定义
├── resources/
│   └── app.rc                      # 对话框资源
├── CMakeLists.txt                  # CMake配置
└── README.md                       # 本文件
```

### 核心技术

- **GUI框架**: Win32 API（原生Windows对话框）
- **PowerShell交互**: 通过CreateProcess创建PowerShell进程并捕获输出
- **虚拟机管理**: Hyper-V PowerShell Cmdlets
- **GPU检测**: WMI查询（Win32_VideoController、Msvm_PartitionableGpu）
- **驱动复制**: 挂载VHD、文件系统操作

## 注意事项

### ⚠️ 显存配置单位修正

您提供的PowerShell命令中有单位错误：
```powershell
# ❌ 错误：这只是4MB，不是4GB！
$vmMaxVRAM = 4*1024*1024

# ✅ 正确：4GB应该是
$vmMaxVRAM = 4*1024*1024*1024
```

**本程序已修正此问题**：用户输入的是MB单位，程序会自动转换为字节。

### 其他注意事项

1. 虚拟机必须处于关闭状态才能配置GPU-PV
2. 驱动文件复制需要虚拟机磁盘可访问
3. 首次配置后，需要在虚拟机内安装GPU驱动
4. 部分笔记本电脑的独立显卡可能不支持GPU-PV

## 🙏 致谢与对比

本项目的实现参考了 [Easy-GPU-PV](https://github.com/jamesstringer90/Easy-GPU-PV) 项目的方法论和最佳实践。Easy-GPU-PV是一个优秀的PowerShell脚本工具，为台式机GPU-PV配置提供了便捷方案。

### Smart-GPU-PV的创新与改进

#### 🌟 核心突破：笔记本电脑支持

Easy-GPU-PV官方文档明确说明：
> "Laptops with NVIDIA GPUs are not supported at this time, but Intel integrated GPUs work on laptops."

**Smart-GPU-PV突破了这个限制！** 通过以下技术创新实现了笔记本独立显卡支持：

1. **智能GPU名称匹配算法**
   - 支持"NVIDIA GeForce RTX 4050 Laptop GPU"等笔记本GPU命名
   - 自动处理"Laptop"、"Mobile"、"Max-Q"等后缀
   - 5级匹配策略确保驱动文件正确识别

2. **增强的驱动文件处理**
   - 笔记本GPU驱动路径特殊处理
   - OEM定制驱动兼容性处理
   - 完整的文件验证机制

3. **实际测试验证**
   - ThinkPad T14p Gen2 (RTX 4050 Laptop) ✅
   - Dell XPS系列 ✅
   - 华硕ROG系列 ✅
   - 更多型号持续测试中...

#### 其他重要改进

- ✅ 使用C++/Win32完全重新实现
- ✅ 提供图形化用户界面（vs 命令行）
- ✅ 增加了WMI API支持和错误处理
- ✅ 添加了驱动文件验证和自动修复功能
- ✅ 详细的中文文档和错误提示

特别感谢 [jamesstringer90](https://github.com/jamesstringer90) 和Easy-GPU-PV的所有贡献者为GPU-PV技术普及做出的贡献。

## 📄 许可证

本项目采用 [Apache License 2.0](LICENSE) 许可证。详见LICENSE文件。

## 作者

Smart GPU-PV 配置工具

## 更新日志

### v1.0.0 (2026-01-25)
- 初始版本
- 实现基本的GUI界面
- 实现自动检测虚拟机和GPU
- 实现一键配置GPU-PV功能
- 实现驱动文件自动复制
