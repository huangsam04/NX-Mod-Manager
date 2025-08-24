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
    // these are tap only
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
    // these can be held
    bool LEFT;
    bool RIGHT;
    bool UP;
    bool DOWN;
    // RIGHT+A组合键相关变量 (RIGHT+A combination key related variables)
    // 简单检测：只要两个键都按下就触发，避免重复触发 (Simple detection: trigger when both keys are pressed, avoid repeated triggering)
    bool RIGHT_AND_A = false; // RIGHT+A组合键状态 (RIGHT+A combination key state)

    static constexpr int MAX = 1000;
    static constexpr int MAX_STEP = 250;
    int step = 50;
    int counter = 0;

    void UpdateButtonHeld(bool& down, bool held);
};

struct AppEntry final {
    std::string name;

    std::string display_version;
    AppID id;
    int image;
    bool selected{false};
    bool own_image{false};
    
    // 缓存的原始图标数据，避免重复从缓存读取
    // Cached raw icon data to avoid repeated cache reads
    std::vector<unsigned char> cached_icon_data;
    bool has_cached_icon{false};
    std::string FILE_NAME;
    std::string FILE_NAME2;
    std::string FILE_PATH; //mods2/游戏名字/ID
    std::string MOD_VERSION;
    std::string MOD_TOTAL;
};


struct MODINFO final {
    std::string MOD_NAME;
    std::string MOD_NAME2;
    std::string MOD_PATH; //mods2/游戏名字/ID/模组的名字
    std::string MOD_TYPE;
    bool MOD_STATE{false};
};

// 资源加载任务结构体
// Resource loading task structure
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

// 资源加载管理器
// Resource loading manager
class ResourceLoadManager {
private:
    // 优先级队列比较器：优先级数值越小，优先级越高
    // Priority queue comparator: lower priority value = higher priority
    struct TaskComparator {
        bool operator()(const ResourceLoadTask& a, const ResourceLoadTask& b) const {
            if (a.priority != b.priority) {
                return a.priority > b.priority; // 优先级高的在前 (Higher priority first)
            }
            return a.submit_time > b.submit_time; // 相同优先级按提交时间排序 (Same priority sorted by submit time)
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


    // 快速获取应用基本信息并缓存图标数据
    // Fast get application basic info and cache icon data
    bool TryGetAppBasicInfoWithIconCache(u64 application_id, AppEntry& entry);
    
    // 分离式扫描：第一阶段快速扫描应用名称
    // Separated scanning: Phase 1 - Fast scan application names
    void FastScanNames(std::stop_token stop_token);
    
    // 基于视口的智能图标加载：根据光标位置优先加载可见区域的图标
    // Viewport-aware smart icon loading: prioritize loading icons in visible area based on cursor position
    void LoadVisibleAreaIcons();

    // 辅助函数：根据filename_path获取其下面的modid和modid下面的子目录名称（使用标准C库）
    // Helper function: Get modid and subdirectory names under filename_path (using standard C library)
    std::pair<u64, std::string> GetModIdAndSubdirCountStdio(const std::string& filename_path);
    
    // 对比mod版本和游戏版本是否一致的辅助函数 (Helper function to compare mod version and game version consistency)
    bool CompareModGameVersion(const std::string& mod_version, const std::string& game_version);
    
    
    
    // 计算当前可见区域的应用索引范围
    // Calculate the index range of applications in current visible area
    std::pair<size_t, size_t> GetVisibleRange() const;
    
    // 上次加载的可见区域范围，用于防抖
    // Last loaded visible range for debouncing
    mutable std::pair<size_t, size_t> last_loaded_range{SIZE_MAX, SIZE_MAX};
    
    // 上次调用LoadVisibleAreaIcons的时间，用于防抖
    // Last time LoadVisibleAreaIcons was called, for debouncing
    mutable std::chrono::steady_clock::time_point last_load_time{};
    
    // 列表界面视口感知图标加载的防抖和缓存机制 (Debouncing and caching mechanism for list interface viewport-aware icon loading)

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
    
    // MOD图标延迟加载标志位 (MOD icons lazy loading flag)
    bool mod_icons_loaded{false};

    util::AsyncFurture<void> async_thread;

    std::mutex mutex{};
    static std::mutex entries_mutex;

    bool finished_scanning{false}; // mutex locked

    static std::atomic<bool> initial_batch_loaded;
    static std::atomic<size_t> scanned_count;
    static std::atomic<size_t> total_count;
    static std::atomic<bool> is_scan_running;
    
    

    // 资源加载管理器
    // Resource loading manager
    ResourceLoadManager resource_manager;
    
    // 每帧资源加载控制
    // Per-frame resource loading control
    std::chrono::steady_clock::time_point last_frame_time;
    bool enable_frame_load_limit{true}; // 是否启用每帧加载限制 (Whether to enable per-frame load limit)

    // this is just bad code, ignore it
    static constexpr float BOX_HEIGHT{120.f};
    float yoff{130.f};
    float ypos{130.f};
    std::size_t start{0};
    std::size_t index{}; // where i am in the array
    
    // MOD列表独立的索引变量 (Independent index variables for MOD list)
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

    // 对话框相关成员变量 (Dialog related member variables)
    DialogType dialog_type{DialogType::INFO};  // 当前对话框类型 (Current dialog type)
    bool show_dialog{false};                   // 是否显示对话框 (Whether to show dialog)
    std::string dialog_content;                // 对话框内容 (Dialog content)
    std::string dialog_button_ok{"确定"};       // 确定按钮文本 (OK button text)
    std::string dialog_button_cancel{"取消"};   // 取消按钮文本 (Cancel button text)
    bool dialog_selected_ok{true};            // 对话框中选中的按钮（true=确定，false=取消）(Selected button in dialog)
    float dialog_content_font_size{18.0f};    // 对话框内容字体大小 (Dialog content font size)
    int dialog_content_align{NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE}; // 内容对齐方式 (Content alignment)

    bool clean_button{false}; // 强制清理标签
    
    // 复制进度相关字段 (Copy progress related fields)
    CopyProgressInfo copy_progress{};           // 复制进度信息 (Copy progress info)
    std::mutex copy_progress_mutex;            // 复制进度互斥锁 (Copy progress mutex)
    util::AsyncFurture<bool> copy_task;        // 异步复制任务 (Async copy task)
    
    float FPS{0.0f}; // 当前帧率 (Current frame rate)
    
    AudioManager audio_manager; // 音效管理器 (Audio manager)

    // 教程界面翻页变量 (Instruction interface pagination variable)
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
    
    // 对话框相关函数 (Dialog related functions)
    /**
     * 显示对话框
     * @param type 对话框类型 (NONE, CONFIRM, INFO, WARNING)
     * @param content 对话框内容文本
     * @param content_font_size 内容字体大小，默认18.0f
     * @param content_align 内容对齐方式，默认居中中间对齐 (NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE)
     * 
     * 对齐方式可选值：
     * 水平对齐: NVG_ALIGN_LEFT, NVG_ALIGN_CENTER, NVG_ALIGN_RIGHT
     * 垂直对齐: NVG_ALIGN_TOP, NVG_ALIGN_MIDDLE, NVG_ALIGN_BOTTOM, NVG_ALIGN_BASELINE
     * 可以使用 | 操作符组合水平和垂直对齐方式
     */
    void ShowDialog(DialogType type, const std::string& content,
                   float content_font_size = 18.0f,
                   int content_align = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    void ShowCopyProgressDialog(const std::string& title); // 显示复制进度对话框 (Show copy progress dialog)
    void UpdateCopyProgress(const CopyProgressInfo& progress); // 更新复制进度 (Update copy progress)
    void HideDialog();
    void DrawDialog();
    void UpdateDialog();

    // 模组名称映射相关函数 (MOD name mapping related functions)
    bool LoadModNameMapping(const std::string& game_name, const std::string& FILE_PATH); // 加载指定游戏的模组名称映射 (Load MOD name mapping for specified game)
    std::string GetMappedModName(const std::string& original_name, const std::string& mod_type); // 获取映射后的模组名称 (Get mapped MOD name)
    std::string GetModDescription(const std::string& original_name, const std::string& mod_type); // 获取模组描述 (Get MOD description)
    void ClearModNameCache(); // 清空映射缓存 (Clear mapping cache)

    // 游戏名称映射相关函数 (Game name mapping related functions)
    bool LoadGameNameMapping(); // 加载游戏名称映射 (Load game name mapping)
    std::string GetMappedGameName(const std::string& original_name, const std::string& mod_version); // 获取映射后的游戏名称 (Get mapped game name)
    void ClearGameNameCache(); // 清空游戏名称映射缓存 (Clear game name mapping cache)

    // 字符串格式化辅助函数 (String formatting helper function)
    std::string GetSnprintf(const std::string& format_str, const std::string& value); // 格式化字符串，将%s替换为指定值 (Format string, replace %s with specified value)

    // MOD相关函数 (MOD related functions)
    void FastScanModInfo(); // 快速扫描MOD信息 (Fast scan MOD info)
    void Sort_Mod(); // 排序MOD列表 (Sort MOD list)
    void ChangeModName(); // 切换当前选中模组的安装状态并修改名称 (Toggle current selected mod install status and modify name)
    int ModInstalled(); // 安装选中的MOD到atmosphere目录 (Install selected MOD to atmosphere directory)
    int ModUninstalled(); // 卸载选中的MOD从atmosphere目录 (Uninstall selected MOD from atmosphere directory)

    bool CopyFile(const std::string& source_path, const std::string& dest_path); // 复制单个文件 (Copy single file)
    
    // 异步复制相关函数 (Async copy related functions)
    bool CopyDirectoryRecursiveAsync(std::stop_token stop_token, const std::string& source_path, const std::string& dest_path, 
                                     std::function<void(const CopyProgressInfo&)> progress_callback); // 异步递归复制目录 (Async recursively copy directory)
    size_t CountFilesInDirectory(const std::string& path); // 统计目录中的文件数量 (Count files in directory)
    
    // 删除目录相关函数 (Directory deletion related functions)
    
    // 选择性删除相关函数 (Selective deletion related functions)
    bool RemoveModFilesSelectively(std::stop_token stop_token, const std::string& mod_source_path, const std::string& target_path,
                                   std::function<void(const CopyProgressInfo&)> progress_callback); // 选择性删除MOD文件 (Selectively remove MOD files)
    size_t CountModFilesToRemove(const std::string& mod_source_path, const std::string& target_path); // 统计需要删除的MOD文件数量 (Count MOD files to remove)




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
    
    // GPU命令优化：双缓冲命令列表用于分离提交和执行
    // GPU command optimization: Double-buffered command lists for separating submission and execution
    static constexpr unsigned NumCommandBuffers = 2;
    dk::UniqueCmdBuf dynamic_cmdbufs[NumCommandBuffers];
    DkCmdList dynamic_cmdlists[NumCommandBuffers];
    unsigned current_cmdbuf_index{0};
    bool command_submitted[NumCommandBuffers]{false, false};
    
    // GPU同步对象用于跟踪命令执行状态
    // GPU synchronization objects for tracking command execution status
    dk::Fence command_fences[NumCommandBuffers];
    void createFramebufferResources();
    void destroyFramebufferResources();
    void recordStaticCommands();
    
    // GPU命令优化相关函数
    // GPU command optimization related functions
    void prepareNextCommandBuffer();
    void submitCurrentCommandBuffer();
    void waitForCommandCompletion(unsigned buffer_index);
};

} // namespace tj
