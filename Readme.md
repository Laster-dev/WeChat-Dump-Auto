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

### C++ 版本

- **语言**: C++
- **平台**: 仅限 Windows
- **功能**: 使用 C++ 标准库和 Windows API 提供高效的内存扫描和基地址查找功能。

### C 版本

- **语言**: C
- **平台**: 仅限 Windows
- **功能**: 通过直接调用 Windows API 实现基础的内存扫描和数据提取，保持代码的简单和高效。

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
   git clone https://github.com/your-repo/WeChatDumpAuto.git
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