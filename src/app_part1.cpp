// 应用程序主头文件 (Main application header file)
#include "app.hpp"
// NanoVG工具函数 (NanoVG utility functions)
#include "nvg_util.hpp"
// NanoVG Deko3D渲染器 (NanoVG Deko3D renderer)
#include "nanovg/deko3d/nanovg_dk.h"
// STB图像库 (STB image library)
#include "nanovg/stb_image.h"
// 语言管理器 (Language manager)
#include "lang_manager.hpp"
// 日志系统 (Logger system)
#include "utils/logger.hpp"
// 时间计算 (Time calculation)
#include <chrono>
// 原子操作 (Atomic operations)
#include <atomic>
// 智能指针 (Smart pointers)
#include <memory>
// C字符串操作 (C string operations)
#include <cstring>
// Nintendo Switch应用缓存库 (Nintendo Switch application cache library)
#include <nxtc_version.h>

// 算法库 (Algorithm library)
#include <algorithm>
// 范围库 (Ranges library)
#include <ranges>
// 迭代器库 (Iterator library)
#include <iterator>
// 目录操作 (Directory operations)
#include <dirent.h>
// 系统调用 (System calls)
#include <cstdlib>
// Switch文件系统API (Switch filesystem API)
#include <switch.h>
// 文件状态 (File status)
#include <sys/stat.h>


// 调试模式日志宏定义 (Debug mode log macro definition)
#ifndef NDEBUG
    #include <cstdio>
    #define LOG(...) std::printf(__VA_ARGS__)
#else // NDEBUG
    #define LOG(...)
#endif // NDEBUG

namespace tj {

// App类的额外成员变量 (Additional member variables for App class)
// 标记首批应用是否已加载完成 (Flag indicating if initial batch of apps is loaded)
std::atomic<bool> App::initial_batch_loaded{false};
// 已扫描的应用数量 (Number of scanned applications)
std::atomic<size_t> App::scanned_count{0};
// 应用总数量 (Total number of applications)
std::atomic<size_t> App::total_count{0};
// 标记扫描是否正在运行 (Flag indicating if scanning is running)
std::atomic<bool> App::is_scan_running{true};
// 保护应用条目列表的互斥锁 (Mutex to protect application entries list)
std::mutex App::entries_mutex;

namespace {





// 感谢Shchmue的贡献 (Thank you Shchmue ^^)
// 应用程序占用空间条目结构体 (Application occupied size entry structure)
struct ApplicationOccupiedSizeEntry {
    std::uint8_t storageId;        // 存储设备ID (Storage device ID)
    std::uint64_t sizeApplication; // 应用程序本体大小 (Application base size)
    std::uint64_t sizePatch;       // 补丁大小 (Patch size)
    std::uint64_t sizeAddOnContent; // 追加内容大小 (Add-on content size)
};

// 应用程序占用空间结构体 (Application occupied size structure)
struct ApplicationOccupiedSize {
    ApplicationOccupiedSizeEntry entry[4]; // 最多4个存储设备的条目 (Up to 4 storage device entries)
};

// 屏幕尺寸常量 (Screen dimension constants)
constexpr float SCREEN_WIDTH = 1280.f;  // 屏幕宽度 (Screen width)
constexpr float SCREEN_HEIGHT = 720.f;  // 屏幕高度 (Screen height)
constexpr int BATCH_SIZE = 4; // 首批加载的应用数量 (Initial batch size for loading apps)

// 验证JPEG数据完整性的辅助函数
// Helper function to validate JPEG data integrity
bool IsValidJpegData(const std::vector<unsigned char>& data) {
    if (data.size() < 4) return false;
    
    // 检查JPEG文件头和文件尾
    // Check JPEG file header and trailer
    bool has_jpeg_header = (data[0] == 0xFF && data[1] == 0xD8);
    bool has_jpeg_trailer = (data[data.size()-2] == 0xFF && data[data.size()-1] == 0xD9);
    return has_jpeg_header && has_jpeg_trailer;
}


// 异步删除应用程序函数 (Asynchronous application deletion function)
// 来自游戏卡安装器的脉冲颜色结构体 (Pulse color structure from gamecard installer)
struct PulseColour {
    NVGcolor col{0, 255, 187, 255}; // 颜色值 (Color value)
    u8 delay;                       // 延迟时间 (Delay time)
    bool increase_blue;             // 是否增加蓝色分量 (Whether to increase blue component)
};

PulseColour pulse; //脉冲颜色对象(Pulse color object)

void update_pulse_colour() { //更新脉冲颜色函数(Update pulse color function)
    if (pulse.col.g == 255) { //当绿色分量达到最大值时(When green component reaches maximum)
        pulse.increase_blue = true; //开始增加蓝色分量(Start increasing blue component)
    } else if (pulse.col.b == 255 && pulse.delay == 10) { //当蓝色分量达到最大值且延迟达到10时(When blue component reaches maximum and delay reaches 10)
        pulse.increase_blue = false; //停止增加蓝色分量(Stop increasing blue component)
        pulse.delay = 0; //重置延迟计数器(Reset delay counter)
    }

    
    
    if (pulse.col.b == 255 && pulse.increase_blue == true) { //当蓝色分量为最大值且正在增加蓝色时(When blue component is at maximum and increasing blue)
        pulse.delay++; //增加延迟计数器(Increment delay counter)
    } else {
        pulse.col.b = pulse.increase_blue ? pulse.col.b + 2 : pulse.col.b - 2; //根据方向调整蓝色分量(Adjust blue component based on direction)
        pulse.col.g = pulse.increase_blue ? pulse.col.g - 2 : pulse.col.g + 2; //根据方向调整绿色分量(Adjust green component based on direction)
    }
}

} // namespace

// 格式化存储大小的辅助函数 (Helper function to format storage size)
// 将字节数转换为可读的存储大小字符串 (Convert bytes to readable storage size string)
std::string FormatStorageSize(std::size_t size_bytes) {
    if (size_bytes == 0) { // 如果大小为0则返回空字符串 (Return empty string if size is 0)
        return "";
    }
    
    // 计算GB值 (Calculate GB value)
    float size_in_gb = static_cast<float>(size_bytes) / 0x40000000; // 1GB = 1024^3 bytes
    
    if (size_in_gb >= 1.0f) { // 如果大于等于1GB则显示GB单位 (Display GB unit if >= 1GB)
        // 显示GB单位，保留一位小数 (Display GB unit with one decimal place)
        char buffer[32]; // 缓冲区用于格式化字符串 (Buffer for formatted string)
        snprintf(buffer, sizeof(buffer), "%.1f GB", size_in_gb); // 格式化为GB字符串 (Format as GB string)
        return std::string(buffer); // 返回格式化后的字符串 (Return formatted string)
    } else {
        // 显示MB单位，保留一位小数 (Display MB unit with one decimal place)
        float size_in_mb = static_cast<float>(size_bytes) / 0x100000; // 1MB = 1024^2 bytes
        char buffer[32]; // 缓冲区用于格式化字符串 (Buffer for formatted string)
        snprintf(buffer, sizeof(buffer), "%.1f MB", size_in_mb); // 格式化为MB字符串 (Format as MB string)
        return std::string(buffer); // 返回格式化后的字符串 (Return formatted string)
    }
}

// ResourceLoadManager 实现 (ResourceLoadManager implementation)
// 资源加载管理器的具体实现 (Concrete implementation of resource load manager)
void ResourceLoadManager::submitLoadTask(const ResourceLoadTask& task) { // 提交加载任务 (Submit loading task)
    std::scoped_lock lock{task_mutex}; // 获取互斥锁保护任务队列 (Acquire mutex lock to protect task queue)
    pending_tasks.push(task); // 将任务添加到待处理队列 (Add task to pending queue)
}

void ResourceLoadManager::processFrameLoads() { // 处理每帧的加载任务 (Process loading tasks per frame)
    int icon_loads_this_frame = 0; // 当前帧已加载的图标数量 (Number of icons loaded in current frame)
    
    std::scoped_lock lock{task_mutex}; // 获取互斥锁保护任务队列 (Acquire mutex lock to protect task queue)
    
    // 处理任务，只对图标任务限制每帧最多2个 (Process tasks, only limit icon tasks to max 2 per frame)
    // 这样可以避免图标加载造成的帧率下降 (This prevents frame rate drops caused by icon loading)
    while (!pending_tasks.empty()) { // 当队列不为空时继续处理 (Continue processing while queue is not empty)
        auto task = pending_tasks.top(); // 获取优先级最高的任务 (Get highest priority task)
        
        // 如果是图标任务且已达到每帧图标加载限制，则停止处理图标任务 (If it's an icon task and we've reached the per-frame icon loading limit, stop processing icon tasks)
        // 但仍然尝试处理其他类型的任务以保持响应性 (But still try to process other types of tasks to maintain responsiveness)
        if (task.task_type == ResourceTaskType::ICON && icon_loads_this_frame >= MAX_ICON_LOADS_PER_FRAME) {
            // 检查是否还有非图标任务可以处理 (Check if there are non-icon tasks that can be processed)
            // 这确保了非图标任务不会被图标任务阻塞 (This ensures non-icon tasks are not blocked by icon tasks)
            std::vector<ResourceLoadTask> temp_tasks; // 临时存储任务的容器 (Temporary container for storing tasks)
            bool found_non_icon = false; // 是否找到非图标任务的标志 (Flag indicating if non-icon task is found)
            
            // 临时移除任务来查找非图标任务 (Temporarily remove tasks to find non-icon tasks)
            // 这是一个线性搜索过程，但任务队列通常不会很大 (This is a linear search process, but task queue is usually not large)
            while (!pending_tasks.empty()) {
                auto temp_task = pending_tasks.top(); // 获取队列顶部任务 (Get top task from queue)
                pending_tasks.pop(); // 从队列中移除任务 (Remove task from queue)
                temp_tasks.push_back(temp_task); // 添加到临时容器 (Add to temporary container)
                
                if (temp_task.task_type != ResourceTaskType::ICON) { // 如果不是图标任务 (If it's not an icon task)
                    found_non_icon = true; // 标记找到非图标任务 (Mark that non-icon task is found)
                    task = temp_task; // 使用找到的非图标任务 (Use the found non-icon task)
                    temp_tasks.pop_back(); // 移除找到的非图标任务 (Remove the found non-icon task)
                    break; // 跳出搜索循环 (Break out of search loop)
                }
            }
            
            // 将临时移除的任务放回队列 (Put temporarily removed tasks back to queue)
            // 保持任务队列的完整性 (Maintain integrity of task queue)
            for (const auto& temp_task : temp_tasks) {
                pending_tasks.push(temp_task); // 重新添加到队列 (Re-add to queue)
            }
            
            if (!found_non_icon) { // 如果没有找到非图标任务 (If no non-icon task is found)
                break; // 没有可处理的任务，退出循环 (No processable tasks, exit loop)
            }
        } else {
            pending_tasks.pop(); // 从队列中移除当前任务 (Remove current task from queue)
        }
        
        // 执行加载任务 (Execute loading task)
        // 调用任务的回调函数来执行实际的加载操作 (Call task's callback function to perform actual loading operation)
        if (task.load_callback) { // 如果回调函数存在 (If callback function exists)
            task.load_callback(); // 执行回调函数 (Execute callback function)
        }
        
        // 只对图标任务计数 (Only count icon tasks)
        // 这样可以准确控制每帧的图标加载数量 (This allows accurate control of icon loading count per frame)
        if (task.task_type == ResourceTaskType::ICON) {
            icon_loads_this_frame++; // 增加图标加载计数 (Increment icon loading count)
        }
    }
}

bool ResourceLoadManager::hasPendingTasks() const { // 检查是否有待处理任务 (Check if there are pending tasks)
    std::scoped_lock lock{task_mutex}; // 获取互斥锁保护任务队列 (Acquire mutex lock to protect task queue)
    return !pending_tasks.empty(); // 返回队列是否非空 (Return whether queue is not empty)
}

size_t ResourceLoadManager::getPendingTaskCount() const { // 获取待处理任务数量 (Get pending task count)
    std::scoped_lock lock{task_mutex}; // 获取互斥锁保护任务队列 (Acquire mutex lock to protect task queue)
    return pending_tasks.size(); // 返回队列大小 (Return queue size)
}

void App::Loop() { // 应用程序主循环方法 (Application main loop method)
    // 60FPS限制：每帧16.666667毫秒 (Frame rate limit: 16.666667ms per frame for 60FPS)
    // 这确保了游戏在Switch上的流畅运行 (This ensures smooth gameplay on Switch)
    constexpr auto target_frame_time = std::chrono::microseconds(16667); // 1/60秒 (1/60 second)
    auto last_frame_time = std::chrono::steady_clock::now(); // 记录上一帧的时间戳 (Record timestamp of last frame)
    
    // 主循环：持续运行直到退出或applet停止 (Main loop: continue running until quit or applet stops)
    // appletMainLoop()检查Switch系统是否允许应用继续运行 (appletMainLoop() checks if Switch system allows app to continue)
    while (!this->quit && appletMainLoop()) {
        auto frame_start = std::chrono::steady_clock::now(); // 记录当前帧开始时间 (Record current frame start time)
        
        // 执行每帧的核心操作 (Execute core operations per frame)
        this->Poll(); // 处理输入事件 (Process input events)
        this->Update(); // 更新游戏逻辑 (Update game logic)
        this->Draw(); // 渲染画面 (Render graphics)
        
        // 计算帧时间并限制到60FPS (Calculate frame time and limit to 60FPS)
        // 这是帧率控制的关键部分 (This is the key part of frame rate control)
        auto frame_end = std::chrono::steady_clock::now(); // 记录当前帧结束时间 (Record current frame end time)
        auto frame_duration = frame_end - frame_start; // 计算帧处理耗时 (Calculate frame processing time)
        
        // 如果帧处理时间少于目标时间，则休眠剩余时间 (If frame processing time is less than target, sleep for remaining time)
        // 这确保了稳定的60FPS帧率 (This ensures stable 60FPS frame rate)
        if (frame_duration < target_frame_time) {
            auto sleep_time = target_frame_time - frame_duration; // 计算需要休眠的时间 (Calculate sleep time needed)
            auto sleep_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(sleep_time).count(); // 转换为纳秒 (Convert to nanoseconds)
            svcSleepThread(sleep_ns); // 使用Switch系统调用进行精确休眠 (Use Switch system call for precise sleep)
        }
        
        // 计算实际FPS (Calculate actual FPS) - 在延时后计算以获得准确的帧时间
        // 这用于性能监控和调试 (This is used for performance monitoring and debugging)
        auto current_time = std::chrono::steady_clock::now(); // 获取当前时间戳 (Get current timestamp)
        auto total_frame_time = current_time - last_frame_time; // 计算完整帧时间（包括休眠） (Calculate complete frame time including sleep)
        auto frame_time_us = std::chrono::duration_cast<std::chrono::microseconds>(total_frame_time).count(); // 转换为微秒 (Convert to microseconds)
        if (frame_time_us > 0) { // 避免除零错误 (Avoid division by zero)
            this->FPS = 1000000.0f / static_cast<float>(frame_time_us); // 转换为FPS (Convert to FPS)
        }
        
        last_frame_time = current_time; // 更新上一帧时间戳 (Update last frame timestamp)
    }
}

void Controller::UpdateButtonHeld(bool& down, bool held) { // 更新按钮长按状态的方法 (Method to update button held state)
    // 处理按钮长按的加速重复逻辑 (Handle accelerated repeat logic for button holding)
    if (down) { // 如果按钮刚被按下 (If button was just pressed)
        this->step = 50; // 重置步长为初始值 (Reset step to initial value)
        this->counter = 0; // 重置计数器 (Reset counter)
    } else if (held) { // 如果按钮持续被按住 (If button is continuously held)
        this->counter += this->step; // 增加计数器 (Increment counter)

        // 当计数器达到阈值时触发重复按下事件 (Trigger repeat press event when counter reaches threshold)
        // 这实现了长按时的加速重复功能 (This implements accelerated repeat functionality during long press)
        if (this->counter >= this->MAX) {
            down = true; // 设置按下状态为真 (Set pressed state to true)
            this->counter = 0; // 重置计数器准备下次重复 (Reset counter for next repeat)
            this->step = std::min(this->step + 50, this->MAX_STEP); // 增加步长实现加速，但不超过最大值 (Increase step for acceleration, but not exceeding maximum)
        }
    }
}

void App::Poll() { // 输入事件轮询方法 (Input event polling method)
    // 输入事件处理时间限制：最大3毫秒，防止复杂输入处理影响帧率 (Input processing time limit: max 3ms to prevent complex input handling from affecting frame rate)
    // 这确保了即使在复杂输入处理时也能维持60FPS (This ensures 60FPS is maintained even during complex input processing)
    constexpr auto max_input_time = std::chrono::microseconds(3000); // 3毫秒 (3 milliseconds)
    auto input_start = std::chrono::steady_clock::now(); // 记录输入处理开始时间 (Record input processing start time)
    
    padUpdate(&this->pad); // 更新手柄状态，从Switch系统获取最新输入数据 (Update controller state, get latest input data from Switch system)

    const auto down = padGetButtonsDown(&this->pad); // 获取本帧新按下的按钮 (Get buttons pressed this frame)
    const auto held = padGetButtons(&this->pad); // 获取当前持续按住的按钮 (Get buttons currently held)

    // 检查是否超时，如果超时则跳过复杂的按键处理 (Check for timeout, skip complex key processing if timed out)
    // 这是第一个超时检查点，确保基础输入获取不会超时 (This is the first timeout checkpoint, ensuring basic input acquisition doesn't timeout)
    auto current_time = std::chrono::steady_clock::now(); // 获取当前时间 (Get current time)
    if (current_time - input_start >= max_input_time) {
        return; // 超时保护，直接返回避免影响帧率 (Timeout protection, return directly to avoid affecting frame rate)
    }

    // 基础按钮状态映射 (Basic button state mapping)
    // 使用位运算检测各个按钮的按下状态 (Use bitwise operations to detect button press states)
    this->controller.A = down & HidNpadButton_A; // A按钮状态 (A button state)
    this->controller.B = down & HidNpadButton_B; // B按钮状态 (B button state)
    this->controller.X = down & HidNpadButton_X; // X按钮状态 (X button state)
    this->controller.Y = down & HidNpadButton_Y; // Y按钮状态 (Y button state)
    this->controller.L = down & HidNpadButton_L; // L肩键状态 (L shoulder button state)
    this->controller.R = down & HidNpadButton_R; // R肩键状态 (R shoulder button state)
    this->controller.L2 = down & HidNpadButton_ZL; // ZL扳机键状态 (ZL trigger button state)
    this->controller.R2 = down & HidNpadButton_ZR; // ZR扳机键状态 (ZR trigger button state)
    this->controller.START = down & HidNpadButton_Plus; // +按钮（开始键）状态 (+ button (start key) state)
    this->controller.SELECT = down & HidNpadButton_Minus; // -按钮（选择键）状态 (- button (select key) state)
    this->controller.RIGHT = down & HidNpadButton_AnyRight; // 右方向键状态 (Right directional key state)
    
    // RIGHT+A组合键检测 (RIGHT+A combination key detection)
    // 简单检测：只要两个键都按下就触发，避免重复触发 (Simple detection: trigger when both keys are pressed, avoid repeated triggering)