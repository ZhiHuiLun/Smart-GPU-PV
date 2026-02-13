# Smart GPU-PV Project Structure | 项目结构说明

## 📁 Directory Structure | 目录结构

```
Smart-GPU-PV/
├── .gitignore                      # Git忽略文件配置
├── .cursor/                        # Cursor IDE配置（已忽略）
├── .vs/                            # Visual Studio配置（已忽略）
├── LICENSE                         # Apache 2.0许可证
├── README.md                       # 项目说明（中文）
├── README_EN.md                    # 项目说明（英文）
├── CONTRIBUTING.md                 # 贡献指南（双语）
├── CHANGELOG.md                    # 更新日志（双语）
├── Smart-GPU-PV.slnx              # Visual Studio解决方案
│
├── docs/                          # 📚 文档目录
│   ├── ARCHITECTURE.md            # 架构和API改进文档
│   └── PROJECT_STRUCTURE.md       # 本文件
│
├── Smart-GPU-PV/                  # 💻 源代码目录
│   ├── *.cpp, *.h                 # C++源代码和头文件
│   ├── *.ico                      # 图标资源
│   ├── *.rc                       # 资源脚本
│   ├── *.vcxproj, *.filters      # Visual Studio项目文件
│   └── x64/                       # 编译输出（已忽略）
│
└── x64/                           # 构建输出目录（已忽略）
```

## 📄 Key Files | 核心文件说明

### Documentation | 文档

| File | Purpose | Language |
|------|---------|----------|
| `README.md` | 项目主文档 | 🇨🇳 中文 |
| `README_EN.md` | 项目主文档 | 🇬🇧 英文 |
| `CONTRIBUTING.md` | 贡献指南 | 🌐 双语 |
| `CHANGELOG.md` | 版本更新日志 | 🌐 双语 |
| `LICENSE` | Apache 2.0许可证 | 🇬🇧 英文 |
| `docs/ARCHITECTURE.md` | 技术架构文档 | 🇨🇳 中文 |
| `docs/PROJECT_STRUCTURE.md` | 项目结构说明 | 🌐 双语 |

### Source Code | 源代码

#### Core Modules | 核心模块

| File | Description |
|------|-------------|
| `Smart-GPU-PV.cpp` | 程序入口点 \| Program entry point |
| `MainWindow.cpp/h` | 主窗口GUI \| Main window interface |
| `GPUPVConfigurator.cpp/h` | GPU-PV配置核心逻辑 \| Core configuration logic |
| `VMManager.cpp/h` | 虚拟机管理 \| VM management (WMI + PowerShell) |
| `GPUManager.cpp/h` | GPU信息管理 \| GPU detection and management |

#### Helper Classes | 辅助类

| File | Description |
|------|-------------|
| `WmiHelper.cpp/h` | WMI操作封装 \| WMI operation wrapper |
| `VhdHelper.cpp/h` | VHD操作封装 \| VHD operation wrapper |
| `PowerShellExecutor.cpp/h` | PowerShell执行器 \| PowerShell executor |
| `Utils.cpp/h` | 工具函数集合 \| Utility functions |
| `HyperVException.h` | 异常处理类 \| Exception handling |

#### Resources | 资源文件

| File | Description |
|------|-------------|
| `Smart-GPU-PV.rc` | 对话框和菜单资源 \| Dialog and menu resources |
| `resource.h` | 资源ID定义 \| Resource ID definitions |
| `*.ico` | 程序图标 \| Application icons |

## 🚀 For Contributors | 贡献者指南

### Adding New Features | 添加新功能

1. **Core Logic**: Modify files in `Smart-GPU-PV/`
2. **Documentation**: Update `README.md`, `README_EN.md`, and `CHANGELOG.md`
3. **Architecture Changes**: Update `docs/ARCHITECTURE.md`

### Documentation Standards | 文档规范

- **README**: 用户使用文档，突出笔记本支持优势
- **CONTRIBUTING**: 贡献流程和代码规范
- **CHANGELOG**: 所有版本变更记录
- **ARCHITECTURE**: 技术实现细节和API说明

### Ignored Files | 被忽略的文件

以下文件/目录不会提交到Git：
- `.vs/`, `.cursor/` - IDE配置
- `x64/`, `Debug/`, `Release/` - 编译输出
- `*.exe`, `*.pdb`, `*.obj` - 二进制文件
- `*.user`, `*.suo` - 个人配置文件

## 🌟 Project Highlights | 项目亮点

### Key Innovation | 核心创新
**全球首个支持笔记本GPU的GPU-PV工具**
- 智能GPU名称匹配
- 5级匹配策略
- 笔记本主流机型验证

### Technical Features | 技术特性
- C++17 + Win32 API
- WMI + PowerShell双重实现
- 完整的错误处理和日志
- 图形化用户界面

## 📋 Checklist for Release | 发布检查清单

在发布新版本前，确保：

- [ ] 更新 `CHANGELOG.md` 版本信息
- [ ] 更新 `README.md` 和 `README_EN.md` 的版本号
- [ ] 测试所有核心功能
- [ ] 确保所有文档是最新的
- [ ] 检查 `.gitignore` 是否正确配置
- [ ] 创建 Git tag（如 `v1.0.0`）

## 🔗 External References | 外部参考

- [Easy-GPU-PV](https://github.com/jamesstringer90/Easy-GPU-PV) - 灵感来源
- [Microsoft Hyper-V Documentation](https://docs.microsoft.com/en-us/virtualization/hyper-v-on-windows/) - 官方文档
- [GPU-PV Technical Overview](https://docs.microsoft.com/en-us/virtualization/hyper-v-on-windows/user-guide/gpu-acceleration) - GPU分区技术

---

**Last Updated**: 2026-02-13
**Version**: 1.0.0
