# 贡献指南 | Contributing Guide

[English](#english) | [中文](#中文)

---

## 中文

感谢您对Smart-GPU-PV项目的关注！我们欢迎所有形式的贡献。

### 🌟 项目愿景

Smart-GPU-PV的目标是让**所有人**（包括笔记本电脑用户）都能轻松使用GPU-PV技术。

#### 为什么创建这个项目？

[Easy-GPU-PV](https://github.com/jamesstringer90/Easy-GPU-PV) 是一个优秀的工具，但它有一个关键限制：
> "Laptops with NVIDIA GPUs are not supported at this time"

许多用户（尤其是移动办公和游戏玩家）使用配备强大独立显卡的笔记本电脑。Smart-GPU-PV通过技术创新，**突破了这个限制**。

### 🤝 如何贡献

#### 1. 报告问题（Bug Reports）

如果您遇到问题，请[创建Issue](../../issues/new)并提供：

- **系统信息**：Windows版本、GPU型号（特别是笔记本型号）
- **详细描述**：发生了什么，预期应该怎样
- **日志输出**：程序界面下方的日志信息
- **复现步骤**：如何重现问题

**我们特别欢迎笔记本GPU相关的问题报告！**

#### 2. 功能建议（Feature Requests）

有好的想法？请[创建Issue](../../issues/new)并说明：

- **功能描述**：您希望添加什么功能
- **使用场景**：为什么需要这个功能
- **预期行为**：功能应该如何工作

#### 3. 笔记本测试报告

**这是最有价值的贡献之一！**

如果您在笔记本电脑上成功（或失败）使用了Smart-GPU-PV，请分享您的经验：

```markdown
**笔记本型号**: ThinkPad T14p Gen2
**GPU型号**: NVIDIA GeForce RTX 4050 Laptop GPU
**驱动版本**: 536.67
**Windows版本**: Windows 11 23H2
**虚拟机系统**: Windows 11 Pro
**测试结果**: ✅ 成功 / ❌ 失败
**备注**: （任何额外信息）
```

#### 4. 代码贡献（Pull Requests）

我们欢迎代码贡献！请遵循以下步骤：

##### 开发环境设置

1. Fork这个仓库
2. 克隆您的Fork：
   ```bash
   git clone https://github.com/YOUR_USERNAME/Smart-GPU-PV.git
   cd Smart-GPU-PV
   ```
3. 创建功能分支：
   ```bash
   git checkout -b feature/your-feature-name
   ```

##### 代码规范

- **语言**: C++ (C++17标准)
- **编码风格**: 
  - 使用4空格缩进
  - 类名使用PascalCase（如`GPUManager`）
  - 函数名使用PascalCase
  - 变量名使用camelCase（如`vmName`）
  - 常量使用UPPER_CASE
- **注释**: 复杂逻辑必须添加注释
- **错误处理**: 使用`HyperVException`处理错误

##### 提交代码

1. 提交您的更改：
   ```bash
   git add .
   git commit -m "feat: 添加XXX功能"
   ```
   
   提交信息格式：
   - `feat:` 新功能
   - `fix:` 修复Bug
   - `docs:` 文档更新
   - `refactor:` 代码重构
   - `test:` 测试相关

2. 推送到您的Fork：
   ```bash
   git push origin feature/your-feature-name
   ```

3. 创建Pull Request到`main`分支

##### Pull Request检查清单

- [ ] 代码遵循项目编码规范
- [ ] 已添加必要的注释
- [ ] 已测试新功能/修复
- [ ] 更新了相关文档（如果需要）
- [ ] 没有引入新的编译警告

#### 5. 文档改进

文档同样重要！您可以：

- 改进README的清晰度
- 添加更多使用示例
- 翻译文档到其他语言
- 添加常见问题解答（FAQ）

### 🎯 特别欢迎的贡献

我们特别需要以下方面的帮助：

- 🎮 **更多笔记本型号测试**（这是最重要的！）
- 🔧 **AMD笔记本GPU支持**（当前主要支持NVIDIA）
- 🌐 **国际化支持**（日语、韩语、德语等）
- 📝 **笔记本特殊配置最佳实践文档**
- 🐛 **Bug修复和性能优化**

### 📜 行为准则

- 尊重所有贡献者
- 保持专业和友好的态度
- 接受建设性的批评
- 关注对项目最有利的事情

### 📞 联系方式

- **Issues**: [GitHub Issues](../../issues)
- **Discussions**: [GitHub Discussions](../../discussions)（如果可用）

### ❤️ 致谢

感谢每一位贡献者，让GPU-PV技术惠及更多用户！

特别感谢：
- [Easy-GPU-PV](https://github.com/jamesstringer90/Easy-GPU-PV)项目为本项目提供的灵感
- 所有报告笔记本测试结果的用户

---

## English

Thank you for your interest in Smart-GPU-PV! We welcome all forms of contributions.

### 🌟 Project Vision

Smart-GPU-PV aims to make GPU-PV technology easily accessible to **everyone**, including laptop users.

#### Why This Project?

[Easy-GPU-PV](https://github.com/jamesstringer90/Easy-GPU-PV) is an excellent tool, but it has a key limitation:
> "Laptops with NVIDIA GPUs are not supported at this time"

Many users (especially mobile workers and gamers) use laptops with powerful discrete GPUs. Smart-GPU-PV **breaks this limitation** through technical innovation.

### 🤝 How to Contribute

#### 1. Bug Reports

If you encounter issues, please [create an Issue](../../issues/new) with:

- **System Information**: Windows version, GPU model (especially laptop models)
- **Detailed Description**: What happened vs. what was expected
- **Log Output**: Log information from the program interface
- **Reproduction Steps**: How to reproduce the issue

**We especially welcome bug reports related to laptop GPUs!**

#### 2. Feature Requests

Have a great idea? [Create an Issue](../../issues/new) and explain:

- **Feature Description**: What feature you'd like to add
- **Use Case**: Why this feature is needed
- **Expected Behavior**: How the feature should work

#### 3. Laptop Testing Reports

**This is one of the most valuable contributions!**

If you've successfully (or unsuccessfully) used Smart-GPU-PV on a laptop, please share your experience:

```markdown
**Laptop Model**: ThinkPad T14p Gen2
**GPU Model**: NVIDIA GeForce RTX 4050 Laptop GPU
**Driver Version**: 536.67
**Windows Version**: Windows 11 23H2
**VM OS**: Windows 11 Pro
**Test Result**: ✅ Success / ❌ Failure
**Notes**: (Any additional information)
```

#### 4. Code Contributions (Pull Requests)

We welcome code contributions! Please follow these steps:

##### Development Environment Setup

1. Fork this repository
2. Clone your fork:
   ```bash
   git clone https://github.com/YOUR_USERNAME/Smart-GPU-PV.git
   cd Smart-GPU-PV
   ```
3. Create a feature branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```

##### Code Standards

- **Language**: C++ (C++17 standard)
- **Coding Style**: 
  - Use 4-space indentation
  - Class names use PascalCase (e.g., `GPUManager`)
  - Function names use PascalCase
  - Variable names use camelCase (e.g., `vmName`)
  - Constants use UPPER_CASE
- **Comments**: Complex logic must be commented
- **Error Handling**: Use `HyperVException` for error handling

##### Submitting Code

1. Commit your changes:
   ```bash
   git add .
   git commit -m "feat: add XXX feature"
   ```
   
   Commit message format:
   - `feat:` New feature
   - `fix:` Bug fix
   - `docs:` Documentation update
   - `refactor:` Code refactoring
   - `test:` Test related

2. Push to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```

3. Create a Pull Request to the `main` branch

##### Pull Request Checklist

- [ ] Code follows project coding standards
- [ ] Added necessary comments
- [ ] Tested new features/fixes
- [ ] Updated relevant documentation (if needed)
- [ ] No new compilation warnings introduced

#### 5. Documentation Improvements

Documentation is equally important! You can:

- Improve README clarity
- Add more usage examples
- Translate documentation to other languages
- Add FAQ section

### 🎯 Especially Welcome Contributions

We particularly need help with:

- 🎮 **More laptop model testing** (This is the most important!)
- 🔧 **AMD laptop GPU support** (Currently mainly supports NVIDIA)
- 🌐 **Internationalization** (Japanese, Korean, German, etc.)
- 📝 **Laptop-specific configuration best practices documentation**
- 🐛 **Bug fixes and performance optimization**

### 📜 Code of Conduct

- Respect all contributors
- Maintain a professional and friendly attitude
- Accept constructive criticism
- Focus on what's best for the project

### 📞 Contact

- **Issues**: [GitHub Issues](../../issues)
- **Discussions**: [GitHub Discussions](../../discussions) (if available)

### ❤️ Acknowledgements

Thank you to all contributors for making GPU-PV technology accessible to more users!

Special thanks to:
- [Easy-GPU-PV](https://github.com/jamesstringer90/Easy-GPU-PV) project for inspiration
- All users who reported laptop testing results

---

**Let's make GPU-PV work on every machine! 🚀**
