#pragma once

#include "nanovg/nanovg.h"
#include "nanovg/deko3d/dk_renderer.hpp"
#include "async.hpp"
#include "audio_manager.hpp"
#include "mod_manager.hpp"
#include "mtp_manager.hpp"
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
#include <unordered_set>

namespace tj {

// 模组名称信息结构体 (MOD name info structure)
struct ModNameInfo {
    std::string display_name; // 显示名称 (Display name)
    std::string description;  // 描述 (Description)
};

using AppID = std::uint64_t;

enum class MenuMode { LOAD, LIST, MODLIST, INSTRUCTION, ADDGAMELIST,MTP};

// 对话框类型枚举 (Dialog type enumeration)
enum class newDialogType {
    CONFIRM,        // 确认对话框 (Confirmation dialog)
    COPY_PROGRESS,  // 复制进度对话框 (Copy progress dialog)
    LIST_SELECT,    // 列表选择对话框 (List select dialog)
    SEARCH,         // 搜索对话框 (Search dialog)
};


// 复制进度信息结构体 (Copy progress info structure)
struct CopyProgressInfo {
    std::string current_file{""};      // 当前正在复制的文件名 (Current file being copied)
    size_t files_copied{0};           // 已复制的文件数量 (Number of files copied)
    size_t total_files{0};            // 需要复制的文件总数 (Total number of files to copy)
    float progress_percentage{0.0f};     // 进度百分比 (Progress percentage)
    bool is_completed{false};             // 是否完成 (Whether completed)
    bool has_error{false};                // 是否有错误 (Whether has error)
    std::string error_message;     // 错误信息 (Error message)
    std::string dialog_title{};  // 对话框标题
    int progress_bar_color[3]{0, 150, 255}; // 进度条颜色 (Progress bar color)

    
    // 文件级进度相关字段 (File-level progress related fields)
    float file_progress_percentage{0.0f}; // 当前文件的复制进度百分比 (Current file copy progress percentage)
    size_t file_bytes_copied{0};          // 当前文件已复制的字节数 (Bytes copied for current file)
    size_t file_total_bytes{0};           // 当前文件的总字节数 (Total bytes for current file)
    bool is_copying_file{false};          // 是否正在复制文件（用于区分删除操作）(Whether copying file - to distinguish from delete operations)
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
    size_t unique_id;
    bool is_favorite{false};
};

struct AppEntry_AddGame final {
    std::string name;
    std::string display_version;
    AppID id;
    int image;
    bool own_image{false};
    
    // 缓存的原始图标数据，避免重复从缓存读取
    // Cached raw icon data to avoid repeated cache reads
    std::vector<unsigned char> cached_icon_data;
    bool has_cached_icon{false};
    size_t unique_id;
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
    size_t unique_id;  // 使用unique_id替代application_id (Use unique_id instead of application_id)
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
    bool TryGetAppBasicInfoWithIconCacheForAddGame(u64 application_id, AppEntry_AddGame& entry); // 专用于AddGame界面的应用信息获取函数 (Dedicated app info function for AddGame interface)
    std::string TryGetAppEnglishName(u64 application_id);
    
    Result GetAllApplicationIds(std::vector<u64>& app_ids);
    
    // 分离式扫描：第一阶段快速扫描应用名称
    // Separated scanning: Phase 1 - Fast scan application names
    void FastScanNames(std::stop_token stop_token);
    
    // 专用异步扫描函数：扫描设备上所有游戏
    // Dedicated async scan function: scan all games on device
    void FastScanAllGames(std::stop_token stop_token);
    
    // 基于视口的智能图标加载：根据光标位置优先加载可见区域的图标
    // Viewport-aware smart icon loading: prioritize loading icons in visible area based on cursor position
    void LoadVisibleAreaIcons();
    void LoadAddGameVisibleAreaIcons(); // 为AddGame界面加载可见区域图标 (Load visible area icons for AddGame interface)

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
    
    // AddGame界面的可见区域缓存和防抖机制 (AddGame interface visible area caching and debouncing mechanism)
    mutable std::pair<size_t, size_t> last_addgame_loaded_range{SIZE_MAX, SIZE_MAX};
    mutable std::chrono::steady_clock::time_point last_addgame_load_time{};
    
    // 列表界面视口感知图标加载的防抖和缓存机制 (Debouncing and caching mechanism for list interface viewport-aware icon loading)

    static constexpr auto LOAD_DEBOUNCE_MS = std::chrono::milliseconds(100); // 防抖延迟100ms (100ms debounce delay)

    NVGcontext* vg{nullptr};
    std::vector<AppEntry> entries;
    std::vector<AppEntry_AddGame> entries_AddGame;
    size_t unique_id{0};
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
    int like_image{};
    
    // MOD图标延迟加载标志位 (MOD icons lazy loading flag)
    bool mod_icons_loaded{false};

    util::AsyncFurture<void> async_thread;

    std::mutex mutex{};
    static std::mutex entries_mutex;
    static std::mutex entries_AddGame_mutex;

    bool finished_scanning{false}; // mutex locked

    static std::atomic<bool> initial_batch_loaded;
    static std::atomic<size_t> scanned_count;
    static std::atomic<size_t> total_count;
    static std::atomic<bool> is_scan_running;
    
    // ADDGAMELIST界面专用的扫描状态变量 (Dedicated scanning state variables for ADDGAMELIST interface)
    static std::atomic<size_t> addgame_scanned_count;
    static std::atomic<size_t> addgame_total_count;
    static std::atomic<bool> addgame_scan_running;
    
    

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
    
    // ADDGAMELIST界面独立的索引变量 (Independent index variables for ADDGAMELIST interface)
    std::size_t index_AddGame{0};  // ADDGAMELIST界面中的当前选中项索引 (Current selected item index in ADDGAMELIST interface)
    std::size_t start_AddGame{0};  // ADDGAMELIST界面显示的起始索引 (Start index for ADDGAMELIST interface display)
    float yoff_AddGame{130.f};     // ADDGAMELIST界面的Y坐标偏移量 (Y coordinate offset for ADDGAMELIST interface)
    float ypos_AddGame{130.f};     // ADDGAMELIST界面的Y坐标位置 (Y coordinate position for ADDGAMELIST interface)
    
    // MOD列表独立的索引变量 (Independent index variables for MOD list)
    std::size_t mod_index{0};  // MOD列表中的当前选中项索引 (Current selected item index in MOD list)
    std::size_t mod_start{0};  // MOD列表显示的起始索引 (Start index for MOD list display)
    float mod_yoff{130.f};     // MOD列表的Y坐标偏移量 (Y coordinate offset for MOD list)
    float mod_ypos{130.f};     // MOD列表的Y坐标位置 (Y coordinate position for MOD list)
    MenuMode menu_mode{MenuMode::LOAD};

    // MTP相关成员变量 (MTP related member variables)
    mtp::MtpManager* mtp_manager{nullptr};  // MTP管理器实例 (MTP manager instance)
    std::vector<std::string> mtp_log_history;  // MTP日志历史记录 (MTP log history)
    mtp::MtpStatus last_mtp_status{mtp::MtpStatus::Stopped};  // 上次MTP状态 (Last MTP status)
    // 注意：已移除last_connection_status和last_transfer_active，因为现在直接使用格式化字符串

    bool quit{false};

    enum class SortType {
        Alphabetical_Reverse,  // Z-A排序 (Z-A sorting)
        Alphabetical,          // A-Z排序 (A-Z sorting)
        MAX,
    };

    enum class SortType_AddGame {
        Alphabetical_Reverse,  // Z-A排序 (Z-A sorting)
        Alphabetical,          // A-Z排序 (A-Z sorting)
        MAX,
    };
    
    int sort_type{0}; 
    int sort_type_AddGame{0};
    
// =======================================新对话框系统======================================================================

// ========================确认对话框======================

    // 对话框回调函数类型定义 (Dialog callback function type definition)
    using DialogConfirmCallback = std::function<void(bool confirmed)>;
    
    // 对话框相关成员变量 (Dialog related member variables)
    newDialogType newdialog_type{newDialogType::CONFIRM};  // 当前对话框类型 (Current dialog type)
    bool show_newdialog{false};                     // 是否显示对话框 (Whether to show dialog)
    std::string newdialog_content{""};                  // 对话框内容 (Dialog content)
    bool newdialog_button{false};            // ture代表确认，false代表取消按钮
    int newdialog_result{-1};            // -1代表没选，0代表取消，1代表确认
    float newdialog_content_font_size{26.0f};    // 对话框内容字体大小 (Dialog content font size)
    int newdialog_content_align{NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE}; // 内容对齐方式 (Content alignment)
    DialogConfirmCallback newdialog_callback{nullptr}; // 对话框确认回调函数 (Dialog confirm callback function)

// ========================确认对话框======================


// ========================列表对话框======================

    // 对话框回调函数类型定义 (Dialog callback function type definition)
    using DialogListSelectCallback = std::function<void(const std::vector<std::string>& selected_items, bool cancelled)>;
    
    // 对话框相关成员变量 (Dialog related member variables)
    std::string newdialog_list_title{""}; // 对话框标题
    std::vector<std::string> newdialog_list_items; // 列表选择项 (List selection items)
    size_t newdialog_list_selected_index{0};      // 当前选中的列表项索引 (Currently selected list item index)
    size_t newdialog_list_start_index{0};         // 列表显示的起始索引，用于分页 (Start index for list display, used for paging)
    bool multiple_select_mode{false}; // 多选模式 (Multi-select mode)
    // 多选功能相关成员变量 (Multi-selection related member variables)
    std::vector<std::string> newdialog_list_selected_items; // 已选中的列表项内容 (Selected list item contents)
    std::unordered_set<size_t> newdialog_list_selected_indices; // 已选中的列表项索引集合 (Set of selected list item indices)
    DialogListSelectCallback newdialog_list_callback{nullptr}; // 对话框确认回调函数 (Dialog confirm callback function)

// ========================列表对话框======================

// ========================进度对话框======================

    std::string newdialog_title{""}; 
    

// ========================进度对话框======================

// ========================搜索对话框======================

using DialogSearchCallback = std::function<void(int result_index)>;
DialogSearchCallback newdialog_search_callback{nullptr};

// 虚拟键盘选中状态相关 (Virtual keyboard selection state related)
    int keyboard_selected_row{0};    // 当前选中的键盘行 (Currently selected keyboard row)
    int keyboard_selected_col{0};    // 当前选中的键盘列 (Currently selected keyboard column)
    
    // 搜索功能相关 (Search functionality related)
    std::vector<std::pair<std::string, int>> search_results{};  // 搜索结果数组：first存储字符串结果，second存储在search_context中的索引位置 (Search results array: first stores string result, second stores index position in search_context)
    std::vector<std::string> search_context{}; // 搜索上下文 (Search context)
    int search_selected_index{0};          // 搜索结果中当前选中的索引 (Currently selected index in search results)
    bool search_has_results{false};        // 是否有搜索结果 (Whether there are search results)
    
    // 触摸状态跟踪变量 (Touch state tracking variables)
    bool touch_locked{false};              // 触摸锁定状态：true=已锁定，false=可触摸 (Touch lock state: true=locked, false=touchable)
    bool last_touch_pressed{false};        // 上一帧的触摸按下状态 (Last frame touch pressed state)
    
    // L形按钮持续触发相关变量 (L-shaped button continuous trigger variables)
    bool right_l_button_held{false};       // 右侧L形按钮是否被按住 (Whether right L-shaped button is held)
    std::chrono::steady_clock::time_point right_l_button_press_start; // 右侧L形按钮按下开始时间 (Right L-shaped button press start time)
    std::chrono::steady_clock::time_point right_l_button_last_trigger; // 右侧L形按钮上次触发时间 (Right L-shaped button last trigger time)
    static constexpr int RIGHT_L_BUTTON_INITIAL_DELAY_MS = 500;  // 初始延迟500ms (Initial delay 500ms)
    static constexpr int RIGHT_L_BUTTON_REPEAT_INTERVAL_MS = 80; // 重复间隔100ms (Repeat interval 80ms)
    
    // 搜索回调延迟执行：复用search_selected_index作为待执行索引，复用last_frame_time作为延迟计时 (Search callback delayed execution: reuse search_selected_index as pending index, reuse last_frame_time for delay timing)
    bool pending_search_callback{false};   // 是否有待执行的搜索回调 (Whether there is a pending search callback)

    // 搜索界面状态管理 (Search interface state management)
    enum class SearchMode {
        KEYBOARD,    // 虚拟键盘模式 (Virtual keyboard mode)
        GAME_LIST    // 游戏列表模式 (Game list mode)
    };
    SearchMode search_mode{SearchMode::KEYBOARD};  // 当前搜索界面模式 (Current search interface mode)
    
    // L形按钮选中状态 (L-shaped button selection state)
    bool left_l_button_selected{false};   // 左侧L形按钮是否被选中 (Whether left L-shaped button is selected)
    bool right_l_button_selected{false};  // 右侧L形按钮是否被选中 (Whether right L-shaped button is selected)

// ========================搜索对话框======================


    void newDrawDialog();
    void newDrawDialogConfirm();
    void newDrawDialogCopyProgress();
    void newDrawDialogListSelect();
    void newDrawDialogBackground();
    void newDrawDialogSearch();
    
    void newUpdateDialog();
    void newUpdateDialogConfirm();
    void newUpdateDialogCopyProgress();
    void newUpdateDialogListSelect();
    void newUpdateDialogSearch();
    void newUpdateTouchDialogSearch();

    void newHideDialog();
    void newHideDialogConfirm();
    void newHideDialogCopyProgress();
    void newHideDialogListSelect();
    void newHideDialogSearch();

    void newShowDialogConfirm(const std::string& newdialog_content,
                                DialogConfirmCallback callback = nullptr,
                                float newdialog_content_font_size = 26.0f,
                                int newdialog_content_align = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    void newShowDialogCopyProgress(const std::string& newdialog_title,
                                const std::string& newdialog_content);

    void newShowDialogListSelect(const std::string& newdialog_list_title,
                                const std::vector<std::string>& newdialog_list_items,
                                bool multiple_select_mode = false,
                                DialogListSelectCallback callback = nullptr,
                                float newdialog_content_font_size = 24.0f);

    void newShowDialogSearch(std::vector<std::string>& search_context, DialogSearchCallback callback = nullptr);

    void newListMenuCenter(const std::string& function);

    // 更新复制进度 (Update copy progress)
    void newUpdateCopyProgress(const CopyProgressInfo& progress);






















// =======================================新对话框系统======================================================================


    // 多选功能相关成员变量 (Multi-selection related member variables)
    std::vector<std::string> dialog_list_selected_items; // 已选中的列表项内容 (Selected list item contents)
    std::unordered_set<size_t> dialog_list_selected_indices; // 已选中的列表项索引集合 (Set of selected list item indices)

    bool clean_button{false}; // 强制清理标签
    
    // 复制进度相关字段 (Copy progress related fields)
    CopyProgressInfo copy_progress{};           // 复制进度信息 (Copy progress info)
    std::mutex copy_progress_mutex;            // 复制进度互斥锁 (Copy progress mutex)
    util::AsyncFurture<bool> copy_task;        // 异步复制任务 (Async copy task)
    util::AsyncFurture<bool> add_task;         // 异步添加MOD任务 (Async add MOD task)
    
    float FPS{0.0f}; // 当前帧率 (Current frame rate)
    
    AudioManager audio_manager; // 音效管理器 (Audio manager)
    
    // 操作计时相关 (Operation timing related)
    std::chrono::milliseconds operation_duration{0}; // 操作耗时 (Operation duration)

    // 教程界面相关变量 (Instruction interface related variables)
    int instruction_image_index{0}; // 当前显示的教程图片索引 (Current instruction image index)
    
    // 三角形透明度动画相关变量 (Triangle opacity animation related variables)
    int triangle_alpha_animation{0};        // 三角形透明度动画状态：0=无动画，1=变亮，2=变暗 (Triangle opacity animation state: 0=no animation, 1=brighten, 2=darken)
    bool triangle_animation_is_left{false}; // 动画方向：true=左侧三角形，false=右侧三角形 (Animation direction: true=left triangle, false=right triangle)
    std::chrono::steady_clock::time_point triangle_animation_start_time; // 三角形动画开始时间 (Triangle animation start time)
    static constexpr int TRIANGLE_ANIMATION_DURATION_MS = 100; // 动画持续时间（毫秒）(Animation duration in milliseconds)
    
    // 边界动画相关变量 (Boundary animation related variables)
    int boundary_flash_animation{0};        // 边界闪烁动画状态：0=无动画，1=闪烁中 (Boundary flash animation state: 0=no animation, 1=flashing)
    bool boundary_animation_is_left{false}; // 边界动画方向：true=左边界，false=右边界 (Boundary animation direction: true=left boundary, false=right boundary)
    std::chrono::steady_clock::time_point boundary_animation_start_time; // 边界动画开始时间 (Boundary animation start time)
    static constexpr int BOUNDARY_ANIMATION_DURATION_MS = 200; // 边界动画持续时间（毫秒）(Boundary animation duration in milliseconds)

    // 模组名称映射相关 (MOD name mapping related)
    std::unordered_map<std::string, ModNameInfo> mod_name_cache; // 缓存已加载的映射数据 (Cache for loaded mapping data)
    std::string current_game_name; // 当前游戏名称 (Current game name)
    
    // 游戏名称映射相关 (Game name mapping related)
    std::unordered_map<std::string, ModNameInfo> game_name_cache; // 缓存已加载的游戏名称映射数据 (Cache for loaded game name mapping data)
    
    // 分类存储的二维数组 (Categorized storage arrays)
    std::vector<std::string> favorites_games_array;     // 喜欢的游戏一维数组，只存储游戏目录名 (Favorites games 1D array, only stores game directory names)
    std::vector<std::string> non_favorites_games_array; // 非喜欢的游戏一维数组，只存储游戏目录名 (Non-favorites games 1D array, only stores game directory names)
    
    // MOD管理器相关 (MOD manager related)
    ModManager mod_manager; // MOD压缩解压管理器 (MOD compression/decompression manager)
    util::AsyncFurture<bool> mod_install_task; // 异步MOD安装任务 (Async MOD installation task)
    bool mod_uninstalling{false}; // MOD卸载是否正在进行 (Whether MOD uninstallation is in progress)
    
    void Draw();
    void Update();
    void Poll();

    void Sort();
    void Sort2(); // ADDGAMELIST界面专用的排序函数 (Dedicated sorting function for ADDGAMELIST interface)
    
    // 排序辅助函数：比较两个AppEntry的排序优先级 (Sorting helper function: compare sorting priority of two AppEntry)
    bool CompareAppEntries(const AppEntry& a, const AppEntry& b, bool reverse_order);
    const char* GetSortStr();
    const char* GetSortStr2();

    std::string GetModJsonPath();
    std::string GetGameDirName();
    std::string GetModDirName();

    void UpdateLoad();
    void UpdateList();
    void UpdateModList(); // 更新MOD列表界面 (Update MOD list interface)
    void UpdateInstruction(); // 更新教程界面 (Update instruction interface)
    void UpdateAddGameList(); // 更新添加游戏列表界面 (Update add game list interface)
    void UpdateMTP(); // 更新MTP界面 (Update MTP interface)

    void DrawBackground();
    void DrawLoad();
    void DrawList();
    void DrawModList(); // 绘制MOD列表界面 (Draw MOD list interface)
    void DrawInstruction(); // 绘制教程界面 (Draw instruction interface)
    void DrawAddGameList(); 
    void DrawMTP(); // 绘制MTP界面 (Draw MTP interface)
    
    
    void Dialog_MODLIST_TYPE_Menu(const std::string& type);// 修改类型界面右侧菜单处理中心 (MOD list interface right menu processing center)
    void Dialog_APPENDMOD_Menu(const std::vector<std::string>& selected_items);// 追加模组界面右侧菜单处理中心 (MOD list interface right menu processing center)
    void Dialog_ADDGAME_Menu(const std::vector<std::string>& selected_items);// 添加游戏界面右侧菜单处理中心 (MOD list interface right menu processing center)
    
    void changegamename(); // 修改游戏名称 (Change game name)
    void changemodversion(); // 修改模组版本 (Change MOD version)
    void changemodname(); // 修改模组名称 (Change MOD name)
    void changemoddescription(); // 修改模组描述 (Change MOD description)
    void tofavorite(); // 标记喜欢 
    void notfavorite(); // 取消标记
    void RemoveMod(); // 移除模组
    void RemoveGame(); // 移除游戏
    

    // 模组名称映射相关函数 (MOD name mapping related functions)
    bool LoadModNameMapping(const std::string& FILE_PATH); // 加载指定游戏的模组名称映射 (Load MOD name mapping for specified game)
    std::string GetMappedModName(const std::string& original_name, const std::string& mod_type); // 获取映射后的模组名称 (Get mapped MOD name)
    std::string GetModDescription(const std::string& original_name, const std::string& mod_type); // 获取模组描述 (Get MOD description)
    void ClearModNameCache(); // 清空映射缓存 (Clear mapping cache)

    // 游戏名称映射相关函数 (Game name mapping related functions)
    bool LoadGameNameMapping(); // 加载游戏名称映射 (Load game name mapping)
    std::string GetMappedGameName(const std::string& original_name, const std::string& mod_version); // 获取映射后的游戏名称 (Get mapped game name)
    void ClearGameNameCache(); // 清空游戏名称映射缓存 (Clear game name mapping cache)
    void FilterSearchResults(const std::string& search_text); // 搜索过滤函数 (Search filter function)
    void clearaddgamelist(); // 清理AddGame界面的所有资源和状态 (Clear all AddGame interface resources and states)
    
    // 测试JSON解析函数：解析game_name.json，生成游戏路径名容器和映射关系 (Test JSON parsing function: parse game_name.json, generate game path container and mapping)
    std::vector<std::string> testjson(std::unordered_map<std::string, std::string>& path_to_name_map);

    // 字符串格式化辅助函数声明 (String formatting helper function declarations)
    std::string GetSnprintf(const std::string& format_str, const std::string& value);
    std::string GetSnprintf(const std::string& format_str, const std::string& value1, const std::string& value2);
    std::string GetSnprintf(const std::string& format_str, const std::string& value1, const std::string& value2, const std::string& value3);
    
    // 格式化时间显示函数
    std::string FormatDuration(double seconds);
    
    // 检测字符串是否100%为英文字符的函数声明 (Function declaration to detect if string is 100% English characters)
    std::string IsEnglishText(const char text[512]);

    // MOD相关函数 (MOD related functions)
    void FastScanModInfo(); // 快速扫描MOD信息 (Fast scan MOD info)
    void Sort_Mod(); // 排序MOD列表 (Sort MOD list)
    void ChangeModName(); // 切换当前选中模组的安装状态并修改名称 (Toggle current selected mod install status and modify name)
    int ModInstalled(); // 安装选中的MOD到atmosphere目录 (Install selected MOD to atmosphere directory)
    int ModUninstalled(); // 卸载选中的MOD从atmosphere目录 (Uninstall selected MOD from atmosphere directory)
    void MODinstallORuninstall();
    
    // 文件系统辅助函数 (Filesystem helper functions)
    bool CheckMods2Path(); // 检查并创建mods2文件夹结构 (Check and create mods2 folder structure)
    std::vector<std::string> appendmodscan(); // 扫描add-mod文件夹中的ZIP文件并返回字符串容器 (Scan ZIP files in add-mod folder and return string vector)
    std::vector<std::string> scanmodgamedir(); 
    
    
    std::string GetFirstCharPinyin(const std::string& chinese_text); // 获取首字符拼音的辅助函数 (Helper function to get first character pinyin)
    
    // 游戏目录排序辅助函数 (Game directory sorting helper function)
    bool gamedirsort(const std::string& game_dir_a, const std::string& game_dir_b);
    
    u32 getgameversioninfo(u64 application_id);

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
