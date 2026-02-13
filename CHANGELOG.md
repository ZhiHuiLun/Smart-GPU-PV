# Changelog | 更新日志

All notable changes to this project will be documented in this file.

本文件记录项目的所有重要变更。

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] - 2026-01-25

### 🌟 Breakthrough Features | 突破性功能

- **世界首个支持笔记本GPU的GPU-PV工具 | World's first GPU-PV tool with laptop GPU support**
  - 智能GPU名称匹配算法，支持笔记本GPU变体 | Intelligent GPU name matching for laptop variants
  - 5级匹配策略（精确→模糊→核心名称→型号数字→最终回退）| 5-tier matching strategy
  - 在ThinkPad T14p Gen2 (RTX 4050 Laptop)等笔记本上测试通过 | Tested on ThinkPad T14p Gen2 and more

### Added | 新增功能

- ✅ **图形化用户界面** | Graphical user interface (Win32 GUI)
  - 直观的对话框操作 | Intuitive dialog-based operation
  - 实时日志显示 | Real-time log display
  - 一键配置按钮 | One-click configuration button

- ✅ **自动检测功能** | Automatic detection
  - 自动检测所有Hyper-V虚拟机 | Auto-detect all Hyper-V VMs
  - 自动识别可分区GPU | Auto-detect partitionable GPUs
  - 显示GPU详细信息（名称、显存、PCI路径）| Display GPU details

- ✅ **智能驱动复制** | Intelligent driver copying
  - 自动挂载虚拟机磁盘 | Auto-mount VM disks
  - 创建HostDriverStore目录 | Create HostDriverStore directory
  - 复制所有必需驱动文件 | Copy all required driver files
  - 完整的文件验证机制 | Comprehensive file verification

- ✅ **WMI API支持** | WMI API support
  - WMI优先，PowerShell作为fallback | WMI first, PowerShell as fallback
  - 可靠的虚拟机状态管理 | Reliable VM state management
  - Virtual Disk API用于VHD操作 | Virtual Disk API for VHD operations

- ✅ **错误处理增强** | Enhanced error handling
  - 统一的`HyperVException`异常类 | Unified `HyperVException` class
  - 详细的错误信息和日志 | Detailed error messages and logging
  - 操作失败时的友好提示 | User-friendly prompts on failures

### Technical Implementation | 技术实现

- **编程语言** | Programming Language: C++ (C++17)
- **GUI框架** | GUI Framework: Win32 API (native Windows dialogs)
- **虚拟机管理** | VM Management: WMI + Hyper-V PowerShell Cmdlets
- **GPU检测** | GPU Detection: WMI (Msvm_PartitionableGpu)
- **驱动复制** | Driver Copying: Virtual Disk API + File operations

### Core Modules | 核心模块

- `MainWindow.cpp/h` - 主窗口界面 | Main window interface
- `GPUPVConfigurator.cpp/h` - GPU-PV配置核心逻辑 | Core GPU-PV configuration logic
- `VMManager.cpp/h` - 虚拟机管理（WMI + PowerShell）| VM management
- `GPUManager.cpp/h` - GPU信息管理 | GPU information management
- `WmiHelper.cpp/h` - WMI操作辅助类 | WMI operation helper
- `VhdHelper.cpp/h` - VHD操作辅助类 | VHD operation helper
- `PowerShellExecutor.cpp/h` - PowerShell命令执行器 | PowerShell command executor
- `Utils.cpp/h` - 工具函数集合 | Utility functions

### Inspired By | 灵感来源

- Implementation approach inspired by [Easy-GPU-PV](https://github.com/jamesstringer90/Easy-GPU-PV)
- 实现方法参考了Easy-GPU-PV项目
- Extended to support laptop GPUs (Easy-GPU-PV limitation)
- 扩展支持笔记本GPU（突破Easy-GPU-PV的限制）

### Known Limitations | 已知限制

- 仅支持Windows 10 20H1+和Windows 11 | Only supports Windows 10 20H1+ and Windows 11
- 需要管理员权限 | Requires administrator privileges
- AMD Polaris GPU（如RX 580）可能不支持硬件视频编码 | AMD Polaris GPUs may not support hardware video encoding
- Vulkan渲染器当前不可用 | Vulkan renderer currently unavailable

### System Requirements | 系统要求

- Windows 10 20H1+ / Windows 11 (Pro/Enterprise/Education)
- Hyper-V功能已启用 | Hyper-V feature enabled
- 支持GPU-PV的显卡 | GPU-PV capable graphics card
- 管理员权限 | Administrator privileges

---

## [Unreleased] | 未发布

### Planned Features | 计划功能

- 🔜 AMD笔记本GPU支持优化 | AMD laptop GPU support optimization
- 🔜 多语言界面支持 | Multi-language UI support
- 🔜 自动驱动更新功能 | Automatic driver update feature
- 🔜 批量VM配置 | Batch VM configuration
- 🔜 配置模板保存/加载 | Configuration template save/load

### Under Consideration | 考虑中

- 💡 集成显卡支持（Intel iGPU on laptops）| Integrated GPU support
- 💡 远程桌面工具推荐和自动配置 | Remote desktop tool recommendation and auto-config
- 💡 虚拟显示驱动集成 | Virtual display driver integration
- 💡 性能监控面板 | Performance monitoring dashboard

---

## Version History | 版本历史

### Version Numbering | 版本编号规则

- **Major (X.0.0)**: 重大功能更新或架构变更 | Major features or architecture changes
- **Minor (0.X.0)**: 新功能添加 | New features added
- **Patch (0.0.X)**: Bug修复和小改进 | Bug fixes and minor improvements

---

## Contributing | 参与贡献

如果您想为项目做出贡献，请查看 [CONTRIBUTING.md](CONTRIBUTING.md)。

If you'd like to contribute to the project, please see [CONTRIBUTING.md](CONTRIBUTING.md).

---

## License | 许可证

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

本项目采用Apache License 2.0许可证 - 详见[LICENSE](LICENSE)文件。

---

**[1.0.0]**: https://github.com/YOUR_USERNAME/Smart-GPU-PV/releases/tag/v1.0.0
