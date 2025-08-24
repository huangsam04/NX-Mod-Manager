# NX Mod Manager

一个专为Nintendo Switch设计的模组管理器，支持多语言界面和音效反馈，提供直观的模组安装、卸载和管理功能。



## 使用方法



### 模组安装流程

1. **扫描检测**: 程序自动扫描SD卡中的模组
2. **版本检查**: 验证模组与游戏版本的兼容性
3. **类型识别**: 根据标签自动识别模组类型
4. **安装确认**: 显示安装进度和结果
5. **状态更新**: 实时更新

### 模组目录结构

```
SD卡/mods2/
└── 游戏名[版本]/
    └── 游戏ID/
        └── 模组名[类型]/
            ├── contents/           # LayeredFS模组
            └── exefs_patches/      # 代码补丁模组
```

### 版本兼容性检查

程序会自动检查模组版本与游戏版本的兼容性：
- ✅ **版本可用**: 模组版本与游戏版本匹配
- ❌ **版本不可用**: 版本不匹配，可能导致问题
- ❓ **版本未知**: 无法确定版本信息



## 项目结构

```
SSM2/
├── src/                          # 源代码目录
│   ├── app.cpp/hpp               # 主应用程序类
│   ├── main.cpp                  # 程序入口点
│   ├── audio_manager.cpp/hpp     # 音效管理器
│   ├── lang_manager.cpp/hpp      # 多语言管理器
│   ├── async.hpp                 # 异步处理工具
│   ├── nanovg/                   # NanoVG图形库
│   ├── yyjson/                   # JSON解析库
│   └── utils/                    # 工具类
│       └── logger.cpp/hpp        # 日志系统
├── assets/                       # 资源文件
│   ├── icon.jpg                  # 应用图标
│   └── romfs/                    # RomFS资源
│       ├── lang/                 # 多语言文件
│       ├── *.jpg                 # 模组类型图标
│       └── shaders/              # 着色器文件
├── lib/                          # 第三方库
│   ├── switch-libpulsar/         # 音频库
│   └── libnxtc-add-version/      # 标题缓存库
├── Makefile                      # 构建配置
└── README.md                     # 项目说明
```


### 环境要求

- **devkitPro**: Switch开发工具链
- **libnx**: Switch系统库
- **C++23编译器**: 支持现代C++特性

### 依赖库

项目使用以下第三方库：

- **deko3d**: Switch GPU图形API
- **switch-libpulsar**: 音频播放库
- **libnxtc-add-version**: 标题缓存管理
- **NanoVG**: 2D矢量图形库
- **yyjson**: 高性能JSON解析库

### 构建步骤

1. **设置环境变量**:
   ```bash
   export DEVKITPRO=/opt/devkitpro
   ```

2. **克隆项目**:
   ```bash
   git clone <repository-url>
   cd SSM2
   ```

3. **编译项目**:
   ```bash
   make
   ```

4. **输出文件**:
   - `NX-Mod-Manager.nro`: 可执行文件
   - `NX-Mod-Manager.nacp`: 应用元数据


## 致谢

感谢以下开源项目和库的贡献：

### 核心依赖
- **[devkitPro](https://devkitpro.org/)** - Nintendo Switch开发工具链
- **[libnx](https://github.com/switchbrew/libnx)** - Switch系统库，提供底层API支持
- **[deko3d](https://github.com/devkitPro/deko3d)** - Switch GPU图形API，实现高性能渲染

### 图形和音频
- **[NanoVG](https://github.com/memononen/nanovg)** - 轻量级2D矢量图形库
- **[switch-libpulsar](https://github.com/Maschell/switch-libpulsar)** - Switch音频播放库
- **[fontstash](https://github.com/memononen/fontstash)** - 字体渲染库

### 工具库
- **[yyjson](https://github.com/ibireme/yyjson)** - 高性能JSON解析库
- **[libnxtc-add-version](https://github.com/DarkMatterCore/libnxtc-add-version)** - 标题缓存管理库
- **[stb](https://github.com/nothings/stb)** - 图像处理库