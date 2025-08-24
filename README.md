# Nintendo Switch Mod Manager (NX-Mod-Manager)

一个功能强大的Nintendo Switch模组管理工具，专为Nintendo Switch自制软件应用程序设计。

## 项目概述

Nintendo Switch Mod Manager是一个专业的模组管理应用程序，运行在Nintendo Switch主机上，为用户提供直观、高效的模组安装、管理和卸载功能。该项目采用现代C++开发，充分利用了Nintendo Switch的硬件特性，提供流畅的用户体验。

## 主要特性

### 🎮 模组管理功能
- **多类型模组支持**：支持Cheat Code、FPS优化、HD材质、美化、游戏性增强等多种模组类型
- **智能安装系统**：自动识别模组结构，支持exefs_patches和contents文件夹的同时安装
- **版本兼容性检查**：自动验证模组与游戏版本的兼容性
- **批量操作**：支持多个模组的批量安装和卸载
- **安全卸载**：完整清理模组文件，避免残留

### 🌐 多语言支持
- 内置多语言系统，支持中文、英文等多种语言
- 动态语言切换，无需重启应用
- 完整的本地化支持，包括UI文本和帮助信息

### 🎵 音效系统
- 集成音效管理器，提供丰富的交互反馈
- 支持系统音效和自定义音效
- 异步音频处理，不影响主界面流畅性

### ⚡ 性能优化
- 异步任务处理框架，避免界面卡顿
- GPU加速渲染，利用deko3d图形API
- 内存优化管理，适配Switch有限的内存资源
- 智能缓存机制，提升响应速度

## 项目结构

```
NX-Mod-Manager/
├── src/                    # 源代码目录
│   ├── app.cpp            # 应用程序主逻辑
│   ├── app.hpp            # 应用程序头文件
│   ├── main.cpp           # 程序入口点
│   ├── async.hpp          # 异步处理框架
│   ├── audio_manager.*    # 音效管理器
│   ├── lang_manager.*     # 多语言管理器
│   ├── nvg_util.*         # NanoVG图形工具
│   ├── utils/             # 工具类
│   │   └── logger.*       # 日志系统
│   ├── nanovg/            # NanoVG图形库
│   └── yyjson/            # JSON解析库
├── assets/                # 资源文件
│   ├── icon.jpg          # 应用图标
│   └── romfs/            # RomFS资源
│       ├── *.jpg         # 模组类型图标
│       ├── lang/         # 语言文件
│       └── shaders/      # 着色器文件
├── lib/                  # 第三方库
│   ├── libnxtc-add-version/  # 版本管理库
│   └── switch-libpulsar/     # Pulsar库
├── Makefile             # 构建配置
├── README.md           # 项目说明
└── LICENSE             # 许可证
```

## 构建说明

### 环境要求
- **devkitPro**：Nintendo Switch开发工具链
- **libnx**：Nintendo Switch系统库
- **deko3d**：图形渲染库
- **C++17**：现代C++标准支持

### 构建步骤
```bash
# 1. 确保devkitPro环境已正确配置
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=${DEVKITPRO}/devkitARM
export DEVKITPPC=${DEVKITPRO}/devkitPPC

# 2. 克隆项目
git clone https://github.com/TOM-BadEN/NX-Mod-Manager.git
cd NX-Mod-Manager

# 3. 编译项目
make

# 4. 清理构建文件（可选）
make clean
```

## 使用方法

### 安装
1. 将编译生成的`.nro`文件复制到Switch的`/switch/`目录
2. 通过Homebrew Launcher启动应用程序

### 操作说明
- **A键**：确认选择/进入
- **B键**：返回/取消
- **十字键/左摇杆**：导航选择
- **R键/右摇杆**：翻页（下一页）
- **L键**：翻页（上一页）
- **+键**：显示帮助信息
- **-键**：退出应用程序

## 许可证

本项目采用MIT许可证，详情请参阅[LICENSE](LICENSE)文件。

## 免责声明

本软件仅供学习和研究目的使用。使用本软件修改游戏可能违反游戏的使用条款，请用户自行承担相关风险和责任。开发者不对因使用本软件而导致的任何损失或法律问题承担责任。

---

**注意**：本项目是一个自制软件，需要在已破解的Nintendo Switch上运行。请确保您了解相关的法律风险，并仅在合法的范围内使用。