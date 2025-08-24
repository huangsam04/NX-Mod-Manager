# NX Mod Manager

一个专为Nintendo Switch设计的模组管理器，支持多语言界面，音效反馈，名称映射，提供直观的模组安装、卸载和管理功能。
MOD采用复制安装，针对大体型的MOD安装速度较慢，但是考虑到方便备份MODS2文件夹，所以采用复制安装的方式。

A mod manager designed specifically for Nintendo Switch, supporting multi-language interface, audio feedback, name mapping, and providing intuitive mod installation, uninstallation, and management functions.
MODs are installed using copy method. For large MODs, the installation speed is slower, but considering the convenience of backing up the MODS2 folder, the copy installation method is adopted.

## 界面展示 | Interface Screenshots

![应用界面1](images/1.jpg)

![应用界面2](images/2.jpg)

![应用界面3](images/3.jpg)

## 使用方法 | Usage

### 完整目录结构图 | Complete Directory Structure

以下是SD卡中模组的完整目录结构示例：

The following is a complete directory structure example of mods in the SD card:

```
SD卡                                         # SD Card
└── mods2
    ├── game_name.json                       # 游戏名称映射文件 (Game name mapping file)
    ├── [游戏名1][版本标签]                   # [Game Name 1][Version Tag]
    │   └── [游戏ID]                         # [Game ID]
    │       ├── mod_name.json                # 模组名称映射文件 (Mod name mapping file)
    │       ├── [模组名称1][模组类型标签]      # [Mod Name 1][Mod Type Tag]
    │       │   ├── contents
    │       │   │   └── [具体模组内容]        # [Specific mod content]
    │       │   └── exefs_patches
    │       │       └── [具体模组内容]        # [Specific mod content]
    │       └── [模组名称2][模组类型标签]      # [Mod Name 2][Mod Type Tag]
    │           ├── contents
    │           │   └── [具体模组内容]        # [Specific mod content]
    │           └── exefs_patches
    │               └── [具体模组内容]        # [Specific mod content]
    │ 
    ├── [游戏名2][版本标签]                   # [Game Name 2][Version Tag]
    │   └── [游戏ID]                         # [Game ID]
    │       ├── mod_name.json                # 模组名称映射文件 (Mod name mapping file)
    │       └── [模组名称][模组类型标签]       # [Mod Name][Mod Type Tag]
    │           ├── contents
    │           │   └── [具体模组内容]        # [Specific mod content]
    │           └── exefs_patches
    │               └── [具体模组内容]        # [Specific mod content]
    │ 
    └── [游戏名3][版本标签]                   # [Game Name 3][Version Tag]
        └── [游戏ID]                         # [Game ID]
            ├── mod_name.json                # 模组名称映射文件 (Mod name mapping file)
            └── [模组名称][模组类型标签]       # [Mod Name][Mod Type Tag]
                ├── contents
                │   └── [具体模组内容]        # [Specific mod content]
                └── exefs_patches
                    └── [具体模组内容]        # [Specific mod content]
```

**目录结构说明 | Directory Structure Description**：
- 每个游戏可以有多个模组 
- Each game can have multiple mods
- 每个模组可以同时包含`contents`和`exefs_patches`两种类型
- Each mod can contain both `contents` and `exefs_patches` types

### 模组目录文字说明 | Mod Directory Text Description

模组使用目录：`SD卡/mods2`

Mod usage directory: `SD Card/mods2`

完整的模组路径格式：

Complete mod path format:
```
mods2/游戏名字[模组版本标签]/游戏ID/模组名字[模组类型标签]/具体模组文件
mods2/Game Name[Mod Version Tag]/Game ID/Mod Name[Mod Type Tag]/Specific mod files
```

**重要说明 | Important Notes**：
- 因为Switch文件系统不支持中文，所以整个路径全部用英文和数字，包括标点符号，否则无法识别
- Because the Switch file system does not support Chinese, the entire path uses English and numbers, including punctuation, otherwise it cannot be recognized
- 游戏ID路径下可放有多个模组，如：模组名字1[模组类型标签]，模组名字2[模组类型标签]
- Multiple mods can be placed under the game ID path, such as: Mod Name 1[Mod Type Tag], Mod Name 2[Mod Type Tag]
- 路径上若不标注标签，不影响实际使用，但建议标注，方便管理
- If tags are not marked on the path, it does not affect actual use, but it is recommended to mark them for easy management

### 标签格式说明 | Tag Format Description

**版本标签格式 | Version Tag Format**：`[版本号 | Version Number]`
- 模组版本标签请按实际模组适用版本填写，只填数字即可
- Please fill in the mod version tag according to the actual applicable version of the mod, only numbers are needed
- 例如：V1.2.0 写成 `[1.2.0]`
- Example: V1.2.0 written as `[1.2.0]`


**模组类型标签| Mod Type Tags**：
- `[F]`：帧率补丁   | Frame rate patch
- `[G]`：图形增强   | Graphics enhancement
- `[B]`：游戏美化   | Game beautification
- `[P]`：更多玩法   | More gameplay
- `[C]`：金手指     | Cheat codes

**注意：** 标签格式为`[X]`，X为标签内容。为了避免识别出错，路径中非标签内容请不要使用`[]`括号。

**Note：** Tag format is `[X]`, where X is the tag content. To avoid recognition errors, please do not use `[]` brackets for non-tag content in the path.

### 模组类型示例 | Mod Type Examples

**Contents模组示例 | Contents Mod Example**：
```
Sentinels[1.0.0]/01008D7016438000/cheat code[C]/contents/01008D7016438000/cheats/F64F574.txt
```

**Exefs_patches模组示例 | Exefs_patches Mod Example**：
```
Bayonetta[1.2.0]/01004A4010FEA000/720&900[G]/exefs_patches/Bayo3/F00DF.ips
```

### 名称映射配置 | Name Mapping Configuration

#### 游戏名称映射 | Game Name Mapping

映射的名字会代替原本文件目录的名字显示在程序里。

The mapped name will replace the original file directory name displayed in the program.

**文件路径 | File Path**：`SD/mods2/game_name.json` 

**格式 | Format**：
```json
{
  "游戏文件夹的名字": "想要映射的名字"
  "Game folder name": "Desired mapped name"
}
```

**示例 | Example**：
```json
{
  "Celeste[1.0.0]": "蔚蓝",
  "Bayonetta 3[1.2.0]": "贝姐3"
}
```

#### 模组名称映射 | Mod Name Mapping

**文件路径 | File Path**：`SD卡/mods2/游戏文件夹的名字[模组版本标签]/游戏ID/mod_name.json`

`SD Card/mods2/Game folder name[Mod version tag]/Game ID/mod_name.json`

**格式 | Format**：
```json
{
  "模组文件夹的名字": {
    "display_name": "想要映射的名字",
    "description": "模组描述"
  }
  "Mod folder name": {
    "display_name": "Desired mapped name",
    "description": "Mod description"
  }
}
```

**示例 | Example**：
```json
{
  "FPS-60[F]": {
    "display_name": "稳定30FPS",
    "description": "更细节的画面调整，帮助游戏稳定30FPS，提高游玩体验。"
  },
  "720&900[G]": {
    "display_name": "更多分辨率",
    "description": "将游戏分辨率调整为手持720p,底座900p，提高画面质量。"
  }
}
```

**注意 | Note**：编辑映射文件时，需注意花括号位置和JSON格式的正确性。

When editing mapping files, pay attention to the position of braces and the correctness of JSON format.

## 项目结构 | Project Structure

```
SSM2/
├── src/                          # 源代码目录 | Source code directory
│   ├── app.cpp/hpp               # 主应用程序类 | Main application class
│   ├── main.cpp                  # 程序入口点 | Program entry point
│   ├── audio_manager.cpp/hpp     # 音效管理器 | Audio manager
│   ├── lang_manager.cpp/hpp      # 多语言管理器 | Multi-language manager
│   ├── async.hpp                 # 异步处理工具 | Async processing tools
│   ├── nanovg/                   # NanoVG图形库 | NanoVG graphics library
│   ├── yyjson/                   # JSON解析库 | JSON parsing library
│   └── utils/                    # 工具类 | Utility classes
│       └── logger.cpp/hpp        # 日志系统 | Logging system
├── assets/                       # 资源文件 | Asset files
│   ├── icon.jpg                  # 应用图标 | Application icon
│   └── romfs/                    # RomFS资源 | RomFS resources
│       ├── lang/                 # 多语言文件 | Multi-language files
│       ├── *.jpg                 # 模组类型图标 | Mod type icons
│       └── shaders/              # 着色器文件 | Shader files
├── lib/                          # 第三方库 | Third-party libraries
│   ├── switch-libpulsar/         # 音频库 | Audio library
│   └── libnxtc-add-version/      # 标题缓存库 | Title cache library
├── Makefile                      # 构建配置 | Build configuration
└── README.md                     # 项目说明 | Project documentation
```

### 构建步骤 | Build Steps

1. **克隆项目 | Clone Project**:
   ```bash
   git clone <https://github.com/TOM-BadEN/NX-Mod-Manager.git>
   cd SSM2
   ```

2. **编译项目 | Compile Project**:
   ```bash
   make
   ```

3. **输出文件 | Output Files**:
   - `NX-Mod-Manager.nro`: 可执行文件 | Executable file

## 致谢 | Acknowledgments

感谢以下开源项目和库的贡献：

Thanks to the following open source projects and libraries for their contributions:

### 核心依赖 | Core Dependencies
- **[devkitPro](https://devkitpro.org/)** - Nintendo Switch开发工具链 | Nintendo Switch development toolchain
- **[libnx](https://github.com/switchbrew/libnx)** - Switch系统库，提供底层API支持 | Switch system library providing low-level API support
- **[deko3d](https://github.com/devkitPro/deko3d)** - Switch GPU图形API，实现高性能渲染 | Switch GPU graphics API for high-performance rendering

### 图形和音频 | Graphics and Audio
- **[NanoVG](https://github.com/memononen/nanovg)** - 轻量级2D矢量图形库 | Lightweight 2D vector graphics library
- **[switch-libpulsar](https://github.com/p-sam/switch-libpulsar)** - Switch音频播放库 | Switch audio playback library
- **[fontstash](https://github.com/memononen/fontstash)** - 字体渲染库 | Font rendering library

### 工具库 | Utility Libraries
- **[yyjson](https://github.com/ibireme/yyjson)** - 高性能JSON解析库 | High-performance JSON parsing library
- **[libnxtc](https://github.com/DarkMatterCore/libnxtc)** - 标题缓存管理库（本项目使用的是添加了version成员的libnxtc-add-version）| Title cache management library (this project uses libnxtc-add-version with added version member)
- **[stb](https://github.com/nothings/stb)** - 图像处理库 | Image processing library