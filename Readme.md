# WeChat Dump Auto

WeChat Dump Auto 是一个用于扫描 WeChat 进程内存并提取用户相关信息的工具。该项目提供了 C#、C++ 和 C 三个版本，所有版本的功能都是一致的，并且都仅适用于 Windows 平台。与其他类似项目相比，WeChat Dump Auto 实现了完全自动化的基地址查找，无需在 WeChat 更新时手动调整地址。

## 功能概述

- **自动基地址查找**: 自动查找 WeChat 进程的基地址，避免了手动更新地址的麻烦。
- **内存扫描与数据提取**: 从 WeChat 进程内存中提取微信用户名、微信 ID、手机号及其他关键数据。
- **多语言实现**: 提供 C#、C++ 和 C 版本，适合不同开发需求，但功能一致。

## 代码版本

### C# 版本

- **语言**: C#
- **平台**: 仅限 Windows
- **功能**: 使用 P/Invoke 调用 Windows API 进行进程操作、内存读取和自动化基地址查找。
- **2024/9/26**: 实现自动化打包压缩，FTP上传功能。
### C++ 版本

- **语言**: C++
- **平台**: 仅限 Windows
- **功能**: 使用 C++ 标准库和 Windows API 提供高效的内存扫描和基地址查找功能。
- **期望**：~~实现自动化打包压缩，FTP上传功能。~~
### C 版本

- **语言**: C
- **平台**: 仅限 Windows
- **功能**: 通过直接调用 Windows API 实现基础的内存扫描和数据提取，保持代码的简单和高效。
- **期望**：~~实现自动化打包压缩，FTP上传功能。~~

## 主要优势

- **自动化基地址查找**: 与其他需要手动更新基地址的工具不同，WeChat Dump Auto 可以在 WeChat 更新后自动查找基地址，免去频繁调整代码的麻烦。
- **跨版本兼容性**: 由于自动化基地址查找的实现，WeChat Dump Auto 能够兼容 WeChat 的多个版本。
- **一致的功能实现**: 无论选择 C#、C++ 还是 C 版本，WeChat Dump Auto 的核心功能和性能保持一致。

## 使用方法

### 环境要求

- Windows 操作系统
- 对应版本的编译器或 IDE（如 Visual Studio）

### 编译和运行

1. **克隆项目**: 

   ```bash
   git clone https://github.com/Laster-dev/WeChat-Dump-Auto.git
   ```
2. **示例输出**：
   ```
   找到匹配字节，用户名地址: WeChatWin.dll+0x12345
   微信ID地址: WeChatWin.dll+0x12367
   手机号地址: WeChatWin.dll+0x12123
   Key地址: WeChatWin.dll+0x12456
   微信ID (文本): wechat123
   手机号 (文本): 1234567890
   Key (十六进制): 4A6F686E446F65
   ```
3. **后续功能**
- [ ] C++/C实现自动化压缩上传
- [ ] 实现数据库可视化
- [ ] 增量获取
# 免责声明

使用 `WeChat Dump Auto`（以下简称“本软件”）即表示您同意以下条款和条件。如果您不同意这些条款，请勿使用本软件。

## 1. 仅供学习和研究

本软件仅供个人学习和研究目的使用。您不得将本软件用于任何非法目的或违反任何适用的法律法规的行为。本软件的开发者不对任何因使用本软件而导致的直接或间接后果负责。

## 2. 责任限制

在任何情况下，本软件的开发者均不对因使用或无法使用本软件而引起的任何形式的损失、损害或法律责任承担责任。这包括但不限于数据丢失、系统故障、财务损失或其他任何形式的损害。

## 3. 第三方软件与数据

本软件可能涉及对第三方软件（如 WeChat 进程）和数据的操作。使用本软件进行的任何此类操作完全由用户自行承担风险。本软件的开发者不对第三方软件的行为或数据的合法性和安全性作任何保证。

## 4. 更新与兼容性

本软件的功能基于特定版本的第三方软件（如 WeChat）。由于第三方软件的更新可能导致本软件功能失效或不兼容，本软件的开发者不承诺对这些变化进行及时更新或提供支持。用户应自行决定是否继续使用本软件并承担相关风险。

## 5. 终止使用

如果您违反本声明中的任何条款，本软件的开发者保留终止您使用本软件的权利。

## 6. 最终解释权

本免责声明的最终解释权归本软件的开发者所有。

---

**注意**: 在您下载、安装或使用本软件之前，请仔细阅读并理解本免责声明的内容。一旦您使用本软件，即表示您同意本免责声明的所有条款。如果您不同意这些条款，请勿使用本软件。
