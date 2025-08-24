#pragma once

#include "nanovg/nanovg.h"
#include "nanovg/deko3d/dk_renderer.hpp"
#include "async.hpp"
#include "audio_manager.hpp"
#include "yyjson/yyjson.h"

#include <switch.h>
#include <cstdint>
#include <vector>
#include <string>
#include <future>
#include <mutex>
#include <optional>
#include <functional>
#include <stop_token>
#include <utility>
#include <queue>
#include <chrono>
#include <unordered_map>

namespace tj {

// 模组名称信息结构体 (MOD name info structure)
struct ModNameInfo {
    std::string display_name; // 显示名称 (Display name)
    std::string description;  // 描述 (Description)
};

using AppID = std::uint64_t;

enum class MenuMode { LOAD, LIST, MODLIST, INSTRUCTION };

// 对话框类型枚举 (Dialog type enumeration)
enum class DialogType {
    INFO,           // 信息对话框 (Information dialog)
    CONFIRM,        // 确认对话框 (Confirmation dialog)
    COPY_PROGRESS,  // 复制进度对话框 (Copy progress dialog)
};

// 复制进度信息结构体 (Copy progress info structure)
struct CopyProgressInfo {
    std::string current_file{"正在统计文件数量..."};      // 当前正在复制的文件名 (Current file being copied)
    size_t files_copied{0};           // 已复制的文件数量 (Number of files copied)
    size_t total_files{0};            // 需要复制的文件总数 (Total number of files to copy)
    float progress_percentage{0.0f};     // 进度百分比 (Progress percentage)
    bool is_completed{false};             // 是否完成 (Whether completed)
    bool has_error{false};                // 是否有错误 (Whether has error)
    std::string error_message;     // 错误信息 (Error message)
};

struct Controller final {
    // 按键状态 (Button states)
    bool A;
    bool B;
    bool X;
    bool Y;
    bool L;
    bool R;
    bool L2;
    bool R2;
    bool START;
    bool SELECT;
    // 方向键状态 (D-pad states)
    bool LEFT;
    bool RIGHT;
    bool UP;
    bool DOWN;
    
    // 组合键状态 (Combination key states)
    bool RIGHT_AND_A = false; // RIGHT+A组合键状态 (RIGHT+A combination key state)

    static constexpr int MAX = 1000;
    static constexpr int MAX_STEP = 250;
    int step = 50;
    int counter = 0;

    void UpdateButtonHeld(bool& down, bool held);
};

struct AppEntry final {
    std::string name;
    // 版本信息 (Version info)
    std::string display_version;
    AppID id;
    int image;
    bool selected{false};
    bool own_image{false};
    
    // 图标缓存相关 (Icon cache related)
    std::vector<unsigned char> cached_icon_data;
    bool has_cached_icon{false};
    std::string FILE_NAME;
    std::string FILE_NAME2;
    std::string FILE_PATH; //mods2/游戏名字/ID
    std::string MOD_VERSION;
    std::string MOD_TOTAL;
};

// MOD信息结构体 (MOD info structure)
struct MODINFO final {
    std::string MOD_NAME;
    std::string MOD_NAME2;
    std::string MOD_PATH; //mods2/游戏名字/ID/模组的名字
    std::string MOD_TYPE;
    bool MOD_STATE{false};
};

// 资源加载任务类型枚举 (Resource loading task type enumeration)
enum class ResourceTaskType {
    ICON     // 图标加载任务 (Icon loading task)
};

struct ResourceLoadTask {
    u64 application_id;
    std::function<void()> load_callback;
    std::chrono::steady_clock::time_point submit_time;
    int priority; // 优先级，数值越小优先级越高 (Priority, lower value = higher priority)
    ResourceTaskType task_type; // 任务类型 (Task type)
};

// 资源加载管理器类 (Resource load manager class)
class ResourceLoadManager {
private:
    // 任务比较器，用于优先队列排序 (Task comparator for priority queue sorting)
    struct TaskComparator {
        bool operator()(const ResourceLoadTask& a, const ResourceLoadTask& b) const {
            // 优先级数值越小，优先级越高 (Lower priority value = higher priority)
            return a.priority > b.priority;
        }
    };
    
    std::priority_queue<ResourceLoadTask, std::vector<ResourceLoadTask>, TaskComparator> pending_tasks;
    mutable std::mutex task_mutex;
    static constexpr int MAX_ICON_LOADS_PER_FRAME = 2;  // 每帧最大图标加载数量 (Max icon loads per frame)
    
public:
    void submitLoadTask(const ResourceLoadTask& task);
    void processFrameLoads(); // 在每帧调用 (Called every frame)
    bool hasPendingTasks() const;
    size_t getPendingTaskCount() const;
};

class App final {
public:
    App();
    ~App();
    void Loop();

private:
    // 应用程序信息获取相关方法 (Application info retrieval related methods)
    // 尝试获取应用基本信息并使用图标缓存 (Try to get app basic info with icon cache)
    bool TryGetAppBasicInfoWithIconCache(u64 application_id, AppEntry& entry);
    
    // 异步扫描应用名称 (Asynchronously scan application names)
    void FastScanNames(std::stop_token stop_token);
    
    // 加载可见区域的图标 (Load icons for visible area)
    void LoadVisibleAreaIcons();
    
    // 获取MOD ID和子目录数量 (Get MOD ID and subdirectory count)
    std::pair<u64, std::string> GetModIdAndSubdirCountStdio(const std::string& filename_path);
    
    // 比较MOD游戏版本 (Compare MOD game version)
    bool CompareModGameVersion(const std::string& mod_version, const std::string& game_version);
    
    // 可见范围管理相关 (Visible range management related)
    // 获取当前可见的应用范围 (Get currently visible application range)
    std::pair<size_t, size_t> GetVisibleRange() const;
    
    // 上次加载的范围缓存 (Last loaded range cache)
    mutable std::pair<size_t, size_t> last_loaded_range{SIZE_MAX, SIZE_MAX};
    
    // 上次加载时间，用于防抖 (Last load time for debouncing)
    mutable std::chrono::steady_clock::time_point last_load_time{};
    
    // 防抖延迟常量 (Debounce delay constant)
    static constexpr auto LOAD_DEBOUNCE_MS = std::chrono::milliseconds(100); // 防抖延迟100ms (100ms debounce delay)

    NVGcontext* vg{nullptr};
    std::vector<AppEntry> entries;
    std::vector<MODINFO> mod_info;

    PadState pad{};
    Controller controller{};
    int default_icon_image{};
    int right_info2{};
    int Cheat_code_MOD_image{};
    int Cosmetic_MOD_image{};
    int FPS_MOD_image{};
    int HD_MOD_image{};
    int More_PLAY_MOD_image{};
    int NONE_MOD_image{};
    
    // MOD图标加载状态 (MOD icon loading status)
    bool mod_icons_loaded{false};

    util::AsyncFurture<void> async_thread;

    std::mutex mutex{};
    static std::mutex entries_mutex;

    bool finished_scanning{false}; // mutex locked

    static std::atomic<bool> initial_batch_loaded;
    static std::atomic<size_t> scanned_count;
    static std::atomic<size_t> total_count;
    static std::atomic<bool> is_scan_running;
    
    // 资源加载管理器 (Resource load manager)
    ResourceLoadManager resource_manager;
    
    // 帧时间管理 (Frame time management)
    std::chrono::steady_clock::time_point last_frame_time;
    bool enable_frame_load_limit{true}; // 是否启用每帧加载限制 (Whether to enable per-frame load limit)

    // UI布局相关常量和变量 (UI layout related constants and variables)
    static constexpr float BOX_HEIGHT{120.f};
    float yoff{130.f};
    float ypos{130.f};
    std::size_t start{0};
    std::size_t index{}; // where i am in the array
    
    // MOD列表相关变量 (MOD list related variables)
    std::size_t mod_index{0};  // MOD列表中的当前选中项索引 (Current selected item index in MOD list)
    std::size_t mod_start{0};  // MOD列表显示的起始索引 (Start index for MOD list display)
    float mod_yoff{130.f};     // MOD列表的Y坐标偏移量 (Y coordinate offset for MOD list)
    float mod_ypos{130.f};     // MOD列表的Y坐标位置 (Y coordinate position for MOD list)
    MenuMode menu_mode{MenuMode::LOAD};

    bool quit{false};

    enum class SortType {
        Alphabetical_Reverse,  // Z-A排序 (Z-A sorting)
        Alphabetical,          // A-Z排序 (A-Z sorting)
        MAX,
    };
    
    int sort_type{0}; 

    // 对话框相关变量 (Dialog related variables)
    DialogType dialog_type{DialogType::INFO};  // 当前对话框类型 (Current dialog type)
    bool show_dialog{false};                   // 是否显示对话框 (Whether to show dialog)
    std::string dialog_content;                // 对话框内容 (Dialog content)
    std::string dialog_button_ok{"确定"};       // 确定按钮文本 (OK button text)
    std::string dialog_button_cancel{"取消"};   // 取消按钮文本 (Cancel button text)
    bool dialog_selected_ok{true};            // 对话框中选中的按钮（true=确定，false=取消）(Selected button in dialog)
    float dialog_content_font_size{18.0f};    // 对话框内容字体大小 (Dialog content font size)
    int dialog_content_align{NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE}; // 内容对齐方式 (Content alignment)

    bool clean_button{false}; // 强制清理标签
    
    // 复制进度相关变量 (Copy progress related variables)
    CopyProgressInfo copy_progress{};           // 复制进度信息 (Copy progress info)
    std::mutex copy_progress_mutex;            // 复制进度互斥锁 (Copy progress mutex)
    util::AsyncFurture<bool> copy_task;        // 异步复制任务 (Async copy task)
    
    float FPS{0.0f}; // 当前帧率 (Current frame rate)
    
    AudioManager audio_manager; // 音效管理器 (Audio manager)

    // 教程界面相关变量 (Instruction interface related variables)
    int instruction_current_page{0};  // 当前页码 (Current page number)

    // 模组名称映射相关 (MOD name mapping related)
    std::unordered_map<std::string, ModNameInfo> mod_name_cache; // 缓存已加载的映射数据 (Cache for loaded mapping data)
    std::string current_game_name; // 当前游戏名称 (Current game name)
    
    // 游戏名称映射相关 (Game name mapping related)
    std::unordered_map<std::string, ModNameInfo> game_name_cache; // 缓存已加载的游戏名称映射数据 (Cache for loaded game name mapping data)
    
    void Draw();
    void Update();
    void Poll();

    void Sort();
    const char* GetSortStr();

    void UpdateLoad();
    void UpdateList();
    void UpdateModList(); // 更新MOD列表界面 (Update MOD list interface)
    void UpdateInstruction(); // 更新教程界面 (Update instruction interface)
    void DrawBackground();
    void DrawLoad();
    void DrawList();
    void DrawModList(); // 绘制MOD列表界面 (Draw MOD list interface)
    void DrawInstruction(); // 绘制教程界面 (Draw instruction interface)
    
    // 对话框相关方法 (Dialog related methods)
    // 显示对话框 (Show dialog)
    // type: 对话框类型 (Dialog type)
    // content: 对话框内容 (Dialog content)
    // content_font_size: 内容字体大小 (Content font size)
    // content_align: 内容对齐方式 (Content alignment)
    void ShowDialog(DialogType type, const std::string& content,
                   float content_font_size = 18.0f,
                   int content_align = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    void ShowCopyProgressDialog(const std::string& title); // 显示复制进度对话框 (Show copy progress dialog)
    void UpdateCopyProgress(const CopyProgressInfo& progress); // 更新复制进度 (Update copy progress)
    void HideDialog();
    void DrawDialog();
    void UpdateDialog();

    // 模组名称映射相关方法 (MOD name mapping related methods)
    bool LoadModNameMapping(const std::string& game_name, const std::string& FILE_PATH); // 加载指定游戏的模组名称映射 (Load MOD name mapping for specified game)
    std::string GetMappedModName(const std::string& original_name, const std::string& mod_type); // 获取映射后的模组名称 (Get mapped MOD name)
    std::string GetModDescription(const std::string& original_name, const std::string& mod_type); // 获取模组描述 (Get MOD description)
    void ClearModNameCache(); // 清空映射缓存 (Clear mapping cache)

    // 游戏名称映射相关方法 (Game name mapping related methods)
    bool LoadGameNameMapping(); // 加载游戏名称映射 (Load game name mapping)
    std::string GetMappedGameName(const std::string& original_name, const std::string& mod_version); // 获取映射后的游戏名称 (Get mapped game name)
    void ClearGameNameCache(); // 清空游戏名称映射缓存 (Clear game name mapping cache)

    // 字符串格式化方法 (String formatting method)
    std::string GetSnprintf(const std::string& format_str, const std::string& value); // 格式化字符串，将%s替换为指定值 (Format string, replace %s with specified value)

    // MOD相关操作方法 (MOD related operation methods)
    void FastScanModInfo(); // 快速扫描MOD信息 (Fast scan MOD info)
    void Sort_Mod(); // 排序MOD列表 (Sort MOD list)
    void ChangeModName(); // 切换当前选中模组的安装状态并修改名称 (Toggle current selected mod install status and modify name)
    int ModInstalled(); // 安装选中的MOD到atmosphere目录 (Install selected MOD to atmosphere directory)
    int ModUninstalled(); // 卸载选中的MOD从atmosphere目录 (Uninstall selected MOD from atmosphere directory)

    bool CopyFile(const std::string& source_path, const std::string& dest_path); // 复制单个文件 (Copy single file)
    
    // 异步文件操作方法 (Async file operation methods)
    bool CopyDirectoryRecursiveAsync(std::stop_token stop_token, const std::string& source_path, const std::string& dest_path, 
                                     std::function<void(const CopyProgressInfo&)> progress_callback); // 异步递归复制目录 (Async recursively copy directory)
    size_t CountFilesInDirectory(const std::string& path); // 统计目录中的文件数量 (Count files in directory)
    
    // MOD文件删除相关方法 (MOD file deletion related methods)
    // 选择性删除MOD文件，只删除与源MOD结构匹配的文件 (Selectively delete MOD files, only delete files matching source MOD structure)
    bool RemoveModFilesSelectively(std::stop_token stop_token, const std::string& mod_source_path, const std::string& target_path,
                                   std::function<void(const CopyProgressInfo&)> progress_callback); // 选择性删除MOD文件 (Selectively remove MOD files)
    size_t CountModFilesToRemove(const std::string& mod_source_path, const std::string& target_path); // 统计需要删除的MOD文件数量 (Count MOD files to remove)

    // Deko3D渲染相关 (Deko3D rendering related)
    // 以下代码来自nanovg deko3d示例，作者adubbz (Code below from nanovg deko3d example by adubbz)
private: // from nanovg decko3d example by adubbz
    static constexpr unsigned NumFramebuffers = 2;
    static constexpr unsigned StaticCmdSize = 0x1000;
    dk::UniqueDevice device;
    dk::UniqueQueue queue;
    std::optional<CMemPool> pool_images;
    std::optional<CMemPool> pool_code;
    std::optional<CMemPool> pool_data;
    dk::UniqueCmdBuf cmdbuf;
    CMemPool::Handle depthBuffer_mem;
    CMemPool::Handle framebuffers_mem[NumFramebuffers];
    dk::Image depthBuffer;
    dk::Image framebuffers[NumFramebuffers];
    DkCmdList framebuffer_cmdlists[NumFramebuffers];
    dk::UniqueSwapchain swapchain;
    DkCmdList render_cmdlist;
    std::optional<nvg::DkRenderer> renderer;
    
    // 动态命令缓冲区管理 (Dynamic command buffer management)
    static constexpr unsigned NumCommandBuffers = 2;
    dk::UniqueCmdBuf dynamic_cmdbufs[NumCommandBuffers];
    DkCmdList dynamic_cmdlists[NumCommandBuffers];
    unsigned current_cmdbuf_index{0};
    bool command_submitted[NumCommandBuffers]{false, false};
    
    // 命令同步相关 (Command synchronization related)
    dk::Fence command_fences[NumCommandBuffers];
    void createFramebufferResources();
    void destroyFramebufferResources();
    void recordStaticCommands();
    
    // 命令缓冲区管理方法 (Command buffer management methods)
    void prepareNextCommandBuffer();
    void submitCurrentCommandBuffer();
    void waitForCommandCompletion(unsigned buffer_index);
};

} // namespace tj