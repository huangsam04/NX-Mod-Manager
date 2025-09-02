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
// 错误码定义 (Error code definitions)
#include <errno.h>


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

// 字符串格式化辅助函数实现 (String formatting helper function implementations)
std::string App::GetSnprintf(const std::string& format_str, const std::string& value) {
    // 计算所需缓冲区大小 (Calculate required buffer size)
    int size = std::snprintf(nullptr, 0, format_str.c_str(), value.c_str());
    if (size <= 0) return format_str; // 格式化失败，返回原字符串 (Format failed, return original string)
    
    // 分配缓冲区并格式化 (Allocate buffer and format)
    std::vector<char> buffer(size + 1);
    std::snprintf(buffer.data(), buffer.size(), format_str.c_str(), value.c_str());
    return std::string(buffer.data());
}

std::string App::GetSnprintf(const std::string& format_str, const std::string& value1, const std::string& value2) {
    // 计算所需缓冲区大小 (Calculate required buffer size)
    int size = std::snprintf(nullptr, 0, format_str.c_str(), value1.c_str(), value2.c_str());
    if (size <= 0) return format_str; // 格式化失败，返回原字符串 (Format failed, return original string)
    
    // 分配缓冲区并格式化 (Allocate buffer and format)
    std::vector<char> buffer(size + 1);
    std::snprintf(buffer.data(), buffer.size(), format_str.c_str(), value1.c_str(), value2.c_str());
    return std::string(buffer.data());
}

// 格式化时间显示函数 (Format duration display function)
std::string App::FormatDuration(double seconds) {
    // 如果小于1秒，
    if (seconds < 1.0) {
        return "0.1s";
    }
    
    int total_seconds = static_cast<int>(seconds);
    int hours = total_seconds / 3600;           // 计算小时数 (Calculate hours)
    int minutes = (total_seconds % 3600) / 60;  // 计算分钟数 (Calculate minutes)
    int secs = total_seconds % 60;              // 计算秒数 (Calculate seconds)
    
    if (hours > 0) {
        // xx时xx分xx秒 (xx hours xx minutes xx seconds)
        return std::to_string(hours) + "h" + std::to_string(minutes) + "m" + std::to_string(secs) + "s";
    } else if (minutes > 0) {
        // xx分xx秒 (xx minutes xx seconds)
        return std::to_string(minutes) + "m" + std::to_string(secs) + "s";
    } else {
        // xx秒 (xx seconds)
        return std::to_string(secs) + "s";
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
    bool right_held = held & HidNpadButton_Right; // 右方向键是否被持续按住 (Whether Right key is being held)
    bool a_held = held & HidNpadButton_A; // A键是否被持续按住 (Whether A key is being held)
    
    // 保存上一帧的组合键状态用于避免重复触发 (Save previous frame combo state to avoid repeated triggering)
    static bool prev_combo_triggered = false;
    
    // 组合键触发条件：两个键都按下且上一帧未触发 (Combination key trigger condition: both keys pressed and not triggered in previous frame)
    if (right_held && a_held && !prev_combo_triggered) {
        this->controller.RIGHT_AND_A = true;
        prev_combo_triggered = true; // 标记已触发，避免重复触发 (Mark as triggered to avoid repeated triggering)
    } else if (!right_held || !a_held) {
        // 任一键释放时重置触发状态 (Reset trigger state when any key is released)
        this->controller.RIGHT_AND_A = false;
        prev_combo_triggered = false;
    } else {
        this->controller.RIGHT_AND_A = false;
    }
    
    // 再次检查时间，避免方向键处理超时 (Check time again to avoid directional key processing timeout)
    // 这是第二个超时检查点，确保方向键处理有足够时间 (This is the second timeout checkpoint, ensuring directional key processing has enough time)
    current_time = std::chrono::steady_clock::now(); // 更新当前时间 (Update current time)
    if (current_time - input_start >= max_input_time) {
        return; // 超时保护 (Timeout protection)
    }
    
    // 方向键状态处理 (Directional key state processing)
    // keep directional keys pressed. 保持方向键按下状态 (Keep directional keys pressed)
    this->controller.DOWN = (down & HidNpadButton_AnyDown); // 下方向键状态 (Down directional key state)
    this->controller.UP = (down & HidNpadButton_AnyUp); // 上方向键状态 (Up directional key state)
    this->controller.LEFT = (down & HidNpadButton_AnyLeft); // 左方向键状态 (Left directional key state)
    

    // 更新方向键的长按状态 (Update directional key held states)
    // 这些调用处理方向键的加速重复功能 (These calls handle accelerated repeat functionality for directional keys)
    this->controller.UpdateButtonHeld(this->controller.DOWN, held & HidNpadButton_AnyDown); // 更新下键长按状态 (Update down key held state)
    this->controller.UpdateButtonHeld(this->controller.UP, held & HidNpadButton_AnyUp); // 更新上键长按状态 (Update up key held state)

#ifndef NDEBUG // 调试模式编译条件 (Debug mode compilation condition)
    // 调试输出也需要时间检查 (Debug output also needs time checking)
    // 确保调试输出不会影响帧率性能 (Ensure debug output doesn't affect frame rate performance)
    current_time = std::chrono::steady_clock::now(); // 获取当前时间用于调试输出时间检查 (Get current time for debug output time checking)
    if (current_time - input_start < max_input_time) { // 只有在时间限制内才执行调试输出 (Only execute debug output within time limit)
        // Lambda函数：用于简化按键状态的调试输出 (Lambda function: simplify debug output for key states)
        // 这个函数只在按键被按下时才输出日志 (This function only outputs log when key is pressed)
        auto display = [](const char* str, bool key) {
            if (key) { // 如果按键被按下 (If key is pressed)
                LOG("Key %s is Pressed\n", str); // 输出按键按下的调试信息 (Output debug info for key press)
            }
        };

        // 调试输出各个按键的状态 (Debug output for each key state)
        // 这些调用帮助开发者了解当前的输入状态 (These calls help developers understand current input state)
        display("A", this->controller.A); // 输出A键状态 (Output A key state)
        display("B", this->controller.B); // 输出B键状态 (Output B key state)
        display("X", this->controller.X); // 输出X键状态 (Output X key state)
        display("Y", this->controller.Y); // 输出Y键状态 (Output Y key state)
        display("L", this->controller.L); // 输出L肩键状态 (Output L shoulder key state)
        display("R", this->controller.R); // 输出R肩键状态 (Output R shoulder key state)
        display("L2", this->controller.L2); // 输出ZL扳机键状态 (Output ZL trigger key state)
        display("R2", this->controller.R2); // 输出ZR扳机键状态 (Output ZR trigger key state)
    }
#endif // 结束调试模式条件编译 (End debug mode conditional compilation)
} // App::Poll()方法结束 (End of App::Poll() method)

// App::Update() - 应用程序主更新方法 (Main application update method)
// 每帧调用一次，处理应用程序的核心逻辑更新 (Called once per frame to handle core application logic updates)
void App::Update() {
    // 每帧处理资源加载（如果启用） (Process resource loading per frame if enabled)
    // 这个机制用于分散资源加载的CPU负担，避免单帧卡顿 (This mechanism distributes CPU load of resource loading to avoid single frame stuttering)
    if (enable_frame_load_limit) { // 检查是否启用了帧限制资源加载 (Check if frame-limited resource loading is enabled)
        resource_manager.processFrameLoads(); // 处理当前帧允许的资源加载任务 (Process resource loading tasks allowed for current frame)
    }
    
    // 根据当前菜单模式执行相应的更新逻辑 (Execute corresponding update logic based on current menu mode)
    // 使用状态机模式管理不同的应用程序状态 (Use state machine pattern to manage different application states)
    switch (this->menu_mode) {
        case MenuMode::LOAD: // 加载模式：应用程序启动时的初始化状态 (Load mode: initialization state during app startup)
            this->UpdateLoad(); // 处理应用程序加载逻辑 (Handle application loading logic)
            break;
        case MenuMode::LIST: // 列表模式：显示应用程序列表的主界面 (List mode: main interface showing application list)
            this->UpdateList(); // 处理应用程序列表的交互和显示 (Handle application list interaction and display)
            // 确保当前屏幕图标始终加载，即使用户不移动光标 (Ensure current screen icons are always loaded, even if user doesn't move cursor)
            this->LoadVisibleAreaIcons();
            break;
        case MenuMode::MODLIST: 
            this->UpdateModList(); 
            break;
        case MenuMode::INSTRUCTION:
            this->UpdateInstruction(); 
            break;

    }
} // App::Update()方法结束 (End of App::Update() method)

// App::Draw() - 应用程序主渲染方法 (Main application rendering method)
// 每帧调用一次，负责所有的GPU渲染操作 (Called once per frame, responsible for all GPU rendering operations)
void App::Draw() {
    // GPU命令优化：准备下一个命令缓冲区 (GPU command optimization: Prepare next command buffer)
    // GPU command optimization: Prepare next command buffer
    // 双缓冲机制，避免GPU等待CPU准备命令 (Double buffering mechanism to avoid GPU waiting for CPU command preparation)
    this->prepareNextCommandBuffer();
    
    // 从交换链获取可用的图像槽位 (Acquire available image slot from swapchain)
    // 这是Vulkan/deko3d渲染管线的标准流程 (This is standard Vulkan/deko3d rendering pipeline procedure)
    const auto slot = this->queue.acquireImage(this->swapchain);
    
    // 提交静态命令（帧缓冲区绑定和基础渲染状态） (Submit static commands - framebuffer binding and basic render state)
    // Submit static commands (framebuffer binding and basic render state)
    // 静态命令在应用启动时预先录制，提高渲染效率 (Static commands are pre-recorded at app startup for rendering efficiency)
    this->queue.submitCommands(this->framebuffer_cmdlists[slot]); // 提交帧缓冲区绑定命令 (Submit framebuffer binding commands)
    this->queue.submitCommands(this->render_cmdlist); // 提交基础渲染状态命令 (Submit basic render state commands)
    
    // 开始记录动态命令到当前命令缓冲区 (Start recording dynamic commands to current command buffer)
    // Start recording dynamic commands to current command buffer
    // 动态命令每帧重新录制，包含实际的绘制调用 (Dynamic commands are re-recorded each frame, containing actual draw calls)
    auto& current_cmdbuf = this->dynamic_cmdbufs[this->current_cmdbuf_index]; // 获取当前帧的命令缓冲区 (Get command buffer for current frame)
    current_cmdbuf.clear(); // 清空上一帧的命令 (Clear commands from previous frame)
    
    // NanoVG渲染命令 (NanoVG rendering commands)
    // NanoVG rendering commands
    // 开始NanoVG帧渲染，设置屏幕尺寸和像素比 (Begin NanoVG frame rendering with screen size and pixel ratio)
    nvgBeginFrame(this->vg, SCREEN_WIDTH, SCREEN_HEIGHT, 1.f);
    
    // 绘制背景元素（标题栏、分割线等） (Draw background elements - title bar, dividers, etc.)
    this->DrawBackground();
    
    // 根据当前菜单模式绘制相应的界面内容 (Draw corresponding interface content based on current menu mode)
    // 使用状态机模式管理不同界面的渲染逻辑 (Use state machine pattern to manage rendering logic for different interfaces)
    switch (this->menu_mode) {
        case MenuMode::LOAD: // 加载界面：显示启动画面和初始化进度 (Load interface: show startup screen and initialization progress)
            this->DrawLoad(); // 绘制加载界面 (Draw loading interface)
            break;
        case MenuMode::LIST: // 列表界面：显示应用程序列表和选择器 (List interface: show application list and selector)
            this->DrawList(); // 绘制应用程序列表 (Draw application list)
            break;
        case MenuMode::MODLIST: 
            this->DrawModList(); 
            break;
        case MenuMode::INSTRUCTION:
            this->DrawInstruction(); 
            break;

    }

    // 结束NanoVG帧渲染，提交所有绘制命令到GPU (End NanoVG frame rendering, submit all draw commands to GPU)
    nvgEndFrame(this->vg);
    
    // 完成动态命令列表记录 (Finish dynamic command list recording)
    // Finish dynamic command list recording
    // 将录制的命令转换为可执行的命令列表 (Convert recorded commands to executable command list)
    this->dynamic_cmdlists[this->current_cmdbuf_index] = current_cmdbuf.finishList();
    
    // 异步提交动态命令（不阻塞CPU） (Asynchronously submit dynamic commands - non-blocking CPU)
    // Asynchronously submit dynamic commands (non-blocking CPU)
    // 允许CPU继续处理下一帧，提高整体性能 (Allow CPU to continue processing next frame for better overall performance)
    this->submitCurrentCommandBuffer();
    
    // 呈现图像 (Present image)
    // Present image
    // 将渲染结果显示到屏幕上 (Display rendering result to screen)
    this->queue.presentImage(this->swapchain, slot);
} // App::Draw()方法结束 (End of App::Draw() method)

// App::DrawBackground() - 绘制应用程序背景界面 (Draw application background interface)
// 包含背景色、分割线、标题和版本信息 (Includes background color, dividers, title and version info)
void App::DrawBackground() {
    
    // 绘制全屏黑色背景 (Draw full-screen black background)
    // 作为所有UI元素的基础背景层 (Serves as base background layer for all UI elements)
    gfx::drawRect(this->vg, 0.f, 0.f, SCREEN_WIDTH, SCREEN_HEIGHT, gfx::Colour::BLACK);
    
    // 绘制顶部分割线 (Draw top divider line)
    // 分离标题区域和主内容区域 (Separate title area from main content area)
    gfx::drawRect(vg, 30.f, 86.0f, 1220.f, 1.f, gfx::Colour::WHITE);
    
    // 绘制底部分割线 (Draw bottom divider line)
    // 分离主内容区域和底部状态栏 (Separate main content area from bottom status bar)
    gfx::drawRect(vg, 30.f, 646.0f, 1220.f, 1.f, gfx::Colour::WHITE);
    
// 版本字符串宏定义 (Version string macro definitions)
// uses the APP_VERSION define in makefile for string version.
// source: https://stackoverflow.com/a/2411008
// 将宏值转换为字符串的标准C预处理器技巧 (Standard C preprocessor trick to convert macro values to strings)
#define STRINGIZE(x) #x // 第一层宏：将参数转换为字符串字面量 (First-level macro: convert parameter to string literal)
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x) // 第二层宏：先展开宏值再转换为字符串 (Second-level macro: expand macro value then convert to string)
    // 在右上角显示应用程序版本号 (Display application version number in top-right corner)
    // 使用银色字体，较小尺寸，不干扰主要内容 (Use silver font, smaller size, doesn't interfere with main content)
    gfx::drawText(this->vg, 70.f, 40.f, 28.f, SOFTWARE_TITLE.c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE);
    gfx::drawText(this->vg, 1224.f, 45.f, 22.f, STRINGIZE_VALUE_OF(UNTITLED_VERSION_STRING), nullptr, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP, gfx::Colour::SILVER);
    
// 清理宏定义，避免污染全局命名空间 (Clean up macro definitions to avoid polluting global namespace)
#undef STRINGIZE
#undef STRINGIZE_VALUE_OF

} // App::DrawBackground()方法结束 (End of App::DrawBackground() method)

// App::DrawLoad() - 绘制加载界面 (Draw loading interface)
// 显示应用程序扫描进度和加载状态信息 (Display application scanning progress and loading status information)
void App::DrawLoad() {
    // 显示扫描进度 (Display scanning progress)
    // 使用作用域锁保护共享数据，确保线程安全 (Use scoped lock to protect shared data for thread safety)
    // 防止在读取loading_text时发生数据竞争 (Prevent data races when reading loading_text)
    std::scoped_lock lock{entries_mutex};
    
    // 在屏幕中央显示加载文本 (Display loading text in screen center)
    // 使用黄色突出显示，36px字体大小，居中对齐 (Use yellow highlight, 36px font size, center alignment)
    // 位置稍微上移40像素，为底部按钮留出空间 (Position slightly moved up 40 pixels to leave space for bottom buttons)
    gfx::drawTextArgs(this->vg, SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f - 40.f, 36.f, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, gfx::Colour::YELLOW, LOADING_TEXT.c_str());
    
    // 在底部显示返回按钮提示 (Display back button hint at bottom)
    // 白色文字，显示B键对应的返回操作 (White text, showing B key corresponding to back operation)
    // 为用户提供退出加载界面的选项 (Provide user with option to exit loading interface)
    gfx::drawButtons(this->vg, gfx::Colour::WHITE, gfx::pair{gfx::Button::B, BUTTON_BACK.c_str()});
} // App::DrawLoad()方法结束 (End of App::DrawLoad() method)


// App::DrawList() - 绘制应用程序列表界面 (Draw application list interface)
// 显示可删除的应用程序列表，包含图标、标题、大小信息和选择状态 (Display deletable application list with icons, titles, size info and selection status)
void App::DrawList() {
    std::scoped_lock lock{entries_mutex}; // 保护entries向量的读取操作 (Protect entries vector read operations)
    // 如果没有应用，显示加载提示 (If no apps, show loading hint)
    // If no apps, show loading hint
    // 处理应用列表为空的边界情况 (Handle edge case when application list is empty)
    if (this->entries.empty()) {
        // 在屏幕中央显示加载提示文本 (Display loading hint text in screen center)
        gfx::drawTextBoxCentered(this->vg, 0.f, 0.f, 1280.f, 720.f, 35.f, 1.5f, NO_FOUND_MOD.c_str(), nullptr, gfx::Colour::SILVER);
        gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_EXIT.c_str()});
        return; // 提前返回，不执行后续的列表绘制逻辑 (Early return, skip subsequent list drawing logic)
    }
    
    SOFTWARE_TITLE = SOFTWARE_TITLE_APP;

    // UI布局常量定义 (UI layout constant definitions)
    // 这些常量定义了列表项和侧边栏的精确布局参数 (These constants define precise layout parameters for list items and sidebar)
    
    // 定义列表项的高度 (120像素) (Define list item height - 120 pixels)
    // 每个应用条目的垂直空间 (Vertical space for each application entry)
    constexpr auto box_height = 120.f;
    
    // 定义列表项的宽度 (715像素) (Define list item width - 715 pixels)
    // 左侧应用列表区域的水平空间 (Horizontal space for left application list area)
    constexpr auto box_width = 715.f;
    
    // 定义图标与边框的间距 (15像素) (Define spacing between icon and border - 15 pixels)
    // 应用图标距离列表项边框的内边距 (Inner padding of app icon from list item border)
    constexpr auto icon_spacing = 15.f;
    
    // 定义标题距离左侧的间距 (116像素) (Define title spacing from left - 116 pixels)
    // 应用标题文本的左侧起始位置 (Left starting position of application title text)
    constexpr auto title_spacing_left = 116.f;
    
    // 定义标题距离顶部的间距 (30像素) (Define title spacing from top - 30 pixels)
    // 应用标题文本的垂直位置 (Vertical position of application title text)
    constexpr auto title_spacing_top = 30.f;
    
    // 定义文本距离左侧的间距 (与标题左侧间距相同) (Define text spacing from left - same as title left spacing)
    // 应用详细信息文本的左侧对齐位置 (Left alignment position for application detail text)
    constexpr auto text_spacing_left = title_spacing_left;
    
    // 定义文本距离顶部的间距 (67像素) (Define text spacing from top - 67 pixels)
    // 应用详细信息文本的垂直位置 (Vertical position of application detail text)
    constexpr auto text_spacing_top = 67.f;
    
    // 定义右侧信息框的X坐标 (870像素) (Define right info box X coordinate - 870 pixels)
    // 右侧信息面板的水平起始位置 (Horizontal starting position of right info panel)
    constexpr auto sidebox_x = 870.f;
    
    // 定义右侧信息框的Y坐标 (87像素) (Define right info box Y coordinate - 87 pixels)
    // 右侧信息面板的垂直起始位置 (Vertical starting position of right info panel)
    constexpr auto sidebox_y = 87.f;
    
    // 定义右侧信息框的宽度 (380像素) (Define right info box width - 380 pixels)
    // 右侧信息面板的水平尺寸 (Horizontal dimension of right info panel)
    constexpr auto sidebox_w = 380.f;
    
    // 定义右侧信息框的高度 (558像素) (Define right info box height - 558 pixels)
    // 右侧信息面板的垂直尺寸 (Vertical dimension of right info panel)
    constexpr auto sidebox_h = 558.f;
    
    // 绘制右侧信息框背景 (Draw right info box background)
    // 注：此处原本尝试使用线性渐变，但未实现 (Note: Originally tried to use linear gradient, but not implemented)
    gfx::drawRect(this->vg, sidebox_x, sidebox_y, sidebox_w, sidebox_h, gfx::Colour::LIGHT_BLACK);

    // 创建并绘制应用图标 (Create and draw application icon)
    const auto right_info2 = nvgImagePattern(this->vg, sidebox_x, sidebox_y, 380.f, 558.f, 0.f, this->right_info2, 1.f);
    gfx::drawRect(this->vg, sidebox_x, sidebox_y, 380.f, 558.f, right_info2);

    // 保存当前绘图状态并设置裁剪区域 (Save current drawing state and set clipping area)
    nvgSave(this->vg);
    nvgScissor(this->vg, 30.f, 86.0f, 1220.f, 646.0f); // 裁剪区域 (Clipping area)

    // 列表项绘制的X坐标常量 (X coordinate constant for list item drawing)
    static constexpr auto x = 90.f;
    // 列表项绘制的Y坐标偏移量 (Y coordinate offset for list item drawing)
    auto y = this->yoff;

    // 遍历并绘制应用列表项 (Iterate and draw application list items)
    for (size_t i = this->start; i < this->entries.size(); i++) {     
        // 检查是否为当前光标选中的项 (Check if this is the currently cursor-selected item)
        if (i == this->index) {
            // 当前选中项：绘制彩色边框和黑色背景 (Current selected item: draw colored border and black background)
            auto col = pulse.col;  // 获取脉冲颜色 (Get pulse color)
            col.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
            col.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
            col.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
            col.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
            update_pulse_colour(); // 更新脉冲颜色(产生闪烁效果) (Update pulse color for blinking effect)
            // 绘制选中项的彩色边框 (Draw colored border for selected item)
            gfx::drawRect(this->vg, x - 5.f, y - 5.f, box_width + 10.f, box_height + 10.f, col);
            // 绘制选中项的黑色背景 (Draw black background for selected item)
            gfx::drawRect(this->vg, x, y, box_width, box_height, gfx::Colour::BLACK);
        }

        // 绘制列表项顶部和底部的分隔线 (Draw top and bottom separator lines for list item)
        gfx::drawRect(this->vg, x, y, box_width, 1.f, gfx::Colour::DARK_GREY);
        gfx::drawRect(this->vg, x, y + box_height, box_width, 1.f, gfx::Colour::DARK_GREY);

        // 创建并绘制应用图标 (Create and draw application icon)
        const auto icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->entries[i].image, 1.f);
        gfx::drawRect(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, icon_paint);

        // 保存当前绘图状态并设置文本裁剪区域 (Save current drawing state and set text clipping area)
        nvgSave(this->vg);
        nvgScissor(this->vg, x + title_spacing_left, y, 585.f, box_height); // clip
        
        
        // 绘制应用名称，防止文本溢出 (Draw application name, preventing text overflow)
        gfx::drawText(this->vg, x + title_spacing_left, y + title_spacing_top, 24.f, this->entries[i].FILE_NAME2.c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE);
        // 恢复之前保存的绘图状态 (Restore previously saved drawing state)
        nvgRestore(this->vg);

        //绘制版本
        
        gfx::drawTextArgs(this->vg, x + text_spacing_left + 0.f, y + text_spacing_top + 9.f, 22.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::SILVER, MOD_COUNT.c_str(), this->entries[i].MOD_TOTAL.c_str());
        gfx::drawTextArgs(this->vg, x + text_spacing_left + 180.f, y + text_spacing_top + 9.f, 22.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::SILVER, MOD_VERSION.c_str(), this->entries[i].MOD_VERSION.c_str());
        gfx::drawTextArgs(this->vg, x + text_spacing_left + 380.f, y + text_spacing_top + 9.f, 22.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::SILVER, GAME_VERSION.c_str(), this->entries[i].display_version.c_str());
        

        // 更新Y坐标为下一项位置 (Update Y coordinate to next item position)
        y += box_height;

        // 超出可视区域时停止绘制 (Stop drawing when out of visible area)
        if ((y + box_height) > 646.f) {
            break;
        }
    }

    
    gfx::drawTextArgs(this->vg, 55.f, 670.f, 24.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE, "%zu / %zu", this->index + 1, this->entries.size());

    // 恢复NanoVG绘图状态 / Restore NanoVG drawing state
    nvgRestore(this->vg);

    
    
    
    
    // 检查扫描状态，动态调整按钮颜色 / Check scan status and dynamically adjust UI element colors
    if (is_scan_running) {
          // 扫描中状态下，根据状态设置按钮颜色 / Set button colors based on scanning state
          // 普通按钮保持白色 / Normal buttons remain white
          gfx::Colour button_color = gfx::Colour::WHITE;
          // Plus和ZR按钮在扫描时变灰，扫描完成后恢复白色 / Plus and ZR buttons turn grey during scan, restore white when complete
          gfx::Colour plus_zr_color = this->is_scan_running ? gfx::Colour::GREY : gfx::Colour::WHITE;

          // 按照drawButtons函数的方式绘制按钮 (Draw buttons following drawButtons function style)
          nvgTextAlign(this->vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP); // 设置文本对齐方式 (Set text alignment)
          float x = 1220.f; // 起始X坐标 (Starting X coordinate)
          const float y = 675.f; // 固定Y坐标 (Fixed Y coordinate)
          float bounds[4]{};

          // 定义所有按钮数组 (Define all buttons array)
          std::array<gfx::pair, 5> buttons = {
              gfx::pair{gfx::Button::B, BUTTON_EXIT.c_str()},
              gfx::pair{gfx::Button::A, BUTTON_SELECT.c_str()},
              gfx::pair{gfx::Button::Y, this->GetSortStr()},
              gfx::pair{gfx::Button::X, ABOUT_BUTTON.c_str()},
              gfx::pair{gfx::Button::PLUS, INSTRUCTION_BUTTON.c_str()}
              
          };

          // 遍历绘制所有按钮 (Iterate and draw all buttons)
          for (const auto& [button, text] : buttons) {
              // 根据按钮类型设置颜色 (Set color based on button type)
              gfx::Colour current_color;
              if (button == gfx::Button::Y) {
                  current_color = plus_zr_color;
              } else {
                  current_color = button_color;
              }
              nvgFillColor(this->vg, gfx::getColour(current_color));

              // 绘制按钮文本 (Draw button text)
              nvgFontSize(this->vg, 20.f);
              nvgTextBounds(this->vg, x, y, text, nullptr, bounds);
              auto text_len = bounds[2] - bounds[0];
              nvgText(this->vg, x, y, text, nullptr);

              // 向左移动文本宽度 (Move left by text width)
              x -= text_len + 10.f;

              // 绘制按钮图标 (Draw button icon)
              nvgFontSize(this->vg, 30.f);
              nvgTextBounds(this->vg, x, y - 7.f, gfx::getButton(button), nullptr, bounds);
              auto icon_len = bounds[2] - bounds[0];
              nvgText(this->vg, x, y - 7.f, gfx::getButton(button), nullptr);

              // 向左移动图标宽度 (Move left by icon width)
              x -= icon_len + 34.f;
          }
        
    } else {
        // 扫描完成，正常显示按钮 (Scan complete, display buttons normally)
        gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_EXIT.c_str()}, 
            gfx::pair{gfx::Button::A, BUTTON_SELECT.c_str()}, 
            gfx::pair{gfx::Button::Y, this->GetSortStr()}, 
            gfx::pair{gfx::Button::X, ABOUT_BUTTON.c_str()},
            gfx::pair{gfx::Button::PLUS, INSTRUCTION_BUTTON.c_str()}
            
            
        );
        
    }
    
    // 绘制对话框（如果显示）(Draw dialog if shown)
    this->DrawDialog();

}


void App::DrawModList() {
    std::scoped_lock lock{entries_mutex}; // 保护entries向量的读取操作 (Protect entries vector read operations)


    // 如果当前游戏没有MOD，显示提示信息 (If current game has no MODs, show hint)
    if (mod_info.empty()) {
        // 在屏幕中央显示加载提示文本 (Display loading hint text in screen center)
        gfx::drawTextBoxCentered(this->vg, 0.f, 0.f, 1280.f, 720.f, 35.f, 1.5f, NO_FOUND_MOD.c_str(), nullptr, gfx::Colour::SILVER);
        gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_BACK.c_str()});
        return; // 提前返回，不执行后续的列表绘制逻辑 (Early return, skip subsequent list drawing logic)
    }

    // UI布局常量定义 (UI layout constant definitions)
    // 这些常量定义了列表项和侧边栏的精确布局参数 (These constants define precise layout parameters for list items and sidebar)
    
    // 定义列表项的高度 (120像素) (Define list item height - 120 pixels)
    // 每个应用条目的垂直空间 (Vertical space for each application entry)
    constexpr auto box_height = 120.f;
    
    // 定义列表项的宽度 (715像素) (Define list item width - 715 pixels)
    // 左侧应用列表区域的水平空间 (Horizontal space for left application list area)
    constexpr auto box_width = 715.f;
    
    // 定义图标与边框的间距 (12像素) (Define spacing between icon and border - 12 pixels)
    // 应用图标距离列表项边框的内边距 (Inner padding of app icon from list item border)
    constexpr auto icon_spacing = 15.f;
    
    // 定义标题距离左侧的间距 (116像素) (Define title spacing from left - 116 pixels)
    // 应用标题文本的左侧起始位置 (Left starting position of application title text)
    constexpr auto title_spacing_left = 116.f;
    
    // 定义标题距离顶部的间距 (30像素) (Define title spacing from top - 30 pixels)
    // 应用标题文本的垂直位置 (Vertical position of application title text)
    constexpr auto title_spacing_top = 30.f;
    
    // 定义文本距离左侧的间距 (与标题左侧间距相同) (Define text spacing from left - same as title left spacing)
    // 应用详细信息文本的左侧对齐位置 (Left alignment position for application detail text)
    constexpr auto text_spacing_left = title_spacing_left;
    
    // 定义文本距离顶部的间距 (67像素) (Define text spacing from top - 67 pixels)
    // 应用详细信息文本的垂直位置 (Vertical position of application detail text)
    constexpr auto text_spacing_top = 67.f;
    
    // 定义右侧信息框的X坐标 (870像素) (Define right info box X coordinate - 870 pixels)
    // 右侧信息面板的水平起始位置 (Horizontal starting position of right info panel)
    constexpr auto sidebox_x = 870.f;
    
    // 定义右侧信息框的Y坐标 (87像素) (Define right info box Y coordinate - 87 pixels)
    // 右侧信息面板的垂直起始位置 (Vertical starting position of right info panel)
    constexpr auto sidebox_y = 87.f;
    
    // 定义右侧信息框的宽度 (380像素) (Define right info box width - 380 pixels)
    // 右侧信息面板的水平尺寸 (Horizontal dimension of right info panel)
    constexpr auto sidebox_w = 380.f;
    
    // 定义右侧信息框的高度 (558像素) (Define right info box height - 558 pixels)
    // 右侧信息面板的垂直尺寸 (Vertical dimension of right info panel)
    constexpr auto sidebox_h = 558.f;
    
    // 绘制右侧信息框背景 (Draw right info box background)
    // 注：此处原本尝试使用线性渐变，但未实现 (Note: Originally tried to use linear gradient, but not implemented)
    gfx::drawRect(this->vg, sidebox_x, sidebox_y, sidebox_w, sidebox_h, gfx::Colour::LIGHT_BLACK);

    // 保存当前绘图状态并设置裁剪区域 (Save current drawing state and set clipping area)
    nvgSave(this->vg);
    nvgScissor(this->vg, 30.f, 86.0f, 1220.f, 646.0f); // 裁剪区域 (Clipping area)

    // 列表项绘制的X坐标常量 (X coordinate constant for list item drawing)
    static constexpr auto x = 90.f;
    // 列表项绘制的Y坐标偏移量 (Y coordinate offset for list item drawing)
    auto y = this->mod_yoff;
    
    // 获取当前选中游戏的MOD信息 (Get MOD information of currently selected game)
    const auto& current_game = this->entries[this->index];
    
    

    // 获取映射后的游戏名称，如果没有映射则使用原始名称 (Get mapped game name, use original name if no mapping)
    // 绘制标题(Draw title)
    SOFTWARE_TITLE = this->entries[this->index].FILE_NAME2;


    // 遍历并绘制当前游戏的MOD列表项 (Iterate and draw MOD list items of current game)
    for (size_t i = this->mod_start; i < mod_info.size(); i++) {
        // 检查是否为当前光标选中的项 (Check if this is the currently cursor-selected item)
        if (i == this->mod_index) {
            // 当前选中项：绘制彩色边框和黑色背景 (Current selected item: draw colored border and black background)
            auto col = pulse.col;  // 获取脉冲颜色 (Get pulse color)
            col.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
            col.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
            col.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
            col.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
            update_pulse_colour(); // 更新脉冲颜色(产生闪烁效果) (Update pulse color for blinking effect)
            // 绘制选中项的彩色边框 (Draw colored border for selected item)
            gfx::drawRect(this->vg, x - 5.f, y - 5.f, box_width + 10.f, box_height + 10.f, col);
            // 绘制选中项的黑色背景 (Draw black background for selected item)
            gfx::drawRect(this->vg, x, y, box_width, box_height, gfx::Colour::BLACK);
        }

        std::string MOD_NAME = mod_info[i].MOD_NAME;
        std::string MOD_TYPE = mod_info[i].MOD_TYPE;
        bool MOD_STATE = mod_info[i].MOD_STATE;

        
        NVGpaint icon_paint; // 声明图标绘制模式变量 (Declare icon paint pattern variable)
        
        switch (MOD_TYPE[1]){
            case 'F':
                MOD_TYPE = FPS_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->FPS_MOD_image, 1.f);
                break;
            case 'G':
                MOD_TYPE = HD_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->HD_MOD_image, 1.f);
                break;
            case 'C':
                MOD_TYPE = CHEAT_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->Cheat_code_MOD_image, 1.f);
                break;
            case 'P':
                MOD_TYPE = PLAY_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->More_PLAY_MOD_image, 1.f);
                break;
            case 'B':
                MOD_TYPE = COSMETIC_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->Cosmetic_MOD_image, 1.f);
                break;
            default:
                MOD_TYPE = NONE_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->NONE_MOD_image, 1.f);
                break;
        }
        
        // 绘制列表项顶部和底部的分隔线 (Draw top and bottom separator lines for list item)
        gfx::drawRect(this->vg, x, y, box_width, 1.f, gfx::Colour::DARK_GREY);
        gfx::drawRect(this->vg, x, y + box_height, box_width, 1.f, gfx::Colour::DARK_GREY);

        

        // 创建并绘制应用图标 (Create and draw application icon)
        // const auto icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->default_icon_image, 1.f);
        gfx::drawRect(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, icon_paint);

        // 保存当前绘图状态并设置文本裁剪区域 (Save current drawing state and set text clipping area)
        nvgSave(this->vg);
        nvgScissor(this->vg, x + title_spacing_left, y, 585.f, box_height); // clip

        // 绘制MOD名称，防止文本溢出 (Draw MOD name, preventing text overflow)
        // 优先显示映射后的名称，如果没有映射则显示原始名称 (Prioritize mapped name, show original if no mapping)
        gfx::drawText(this->vg, x + title_spacing_left, y + title_spacing_top, 24.f, mod_info[i].MOD_NAME2.c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE);
        // 恢复之前保存的绘图状态 (Restore previously saved drawing state)
        nvgRestore(this->vg);

        // 绘制版本信息 (Draw version information)
        gfx::drawTextArgs(this->vg, x + text_spacing_left + 0.f, y + text_spacing_top + 9.f, 22.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::SILVER, MOD_TYPE_TEXT.c_str(), MOD_TYPE.c_str());

        // 绘制模组状态 (Draw mod status)
        if (MOD_STATE){
            gfx::drawTextArgs(this->vg, 755.f, y + box_height / 2.f, 26.f, 
                NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, gfx::Colour::CYAN, MOD_STATE_INSTALLED.c_str());
        }else{
            gfx::drawTextArgs(this->vg, 755.f, y + box_height / 2.f, 26.f, 
                NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, gfx::Colour::SILVER, MOD_STATE_UNINSTALLED.c_str());
        }
        

        
        // 更新Y坐标为下一项位置 (Update Y coordinate to next item position)
        y += box_height;

        // 超出可视区域时停止绘制 (Stop drawing when out of visible area)
        if ((y + box_height) > 646.f) {
            break;
        }
    }



    // 绘制当前选中游戏的图标到右侧信息框左上角 (Draw current selected game icon to top-left of right info box)
    // 计算图标绘制位置：信息框左上角 + 偏移量 (Calculate icon position: info box top-left + offset)
    const float icon_x = sidebox_x + 35.f;  // 距离信息框左边35像素 (35 pixels from left edge of info box)
    const float icon_y = sidebox_y + 35.f;  // 距离信息框顶部35像素 (35 pixels from top edge of info box)
    const float icon_size = 130.f;          // 图标尺寸 (Icon size)
        
    // 绘制半透明背景遮罩层，增加层次感 (Draw semi-transparent background mask for depth)
    const NVGcolor mask_color = nvgRGBA(0, 0, 0, 30);  // 半透明黑色遮罩 (Semi-transparent black mask)
    gfx::drawRect(this->vg, icon_x - 2.f, icon_y - 2.f, icon_size + 4.f, icon_size + 4.f, mask_color);
        
    // 创建当前选中游戏的图标绘制模式，使用轻微透明度 (Create image pattern with slight transparency)
    const auto selected_icon_paint = nvgImagePattern(this->vg, icon_x, icon_y, icon_size, icon_size, 0.f, current_game.image, 0.9f);
        
    // 绘制当前选中游戏的图标 (Draw current selected game icon)
    gfx::drawRect(this->vg, icon_x, icon_y, icon_size, icon_size, selected_icon_paint);

    // 绘制版本检测状态 (Draw version staus)
    gfx::drawTextArgs(this->vg, icon_x + icon_size + 30.f, icon_y + icon_size / 4 -10.f, 
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE, GAME_VERSION.c_str(), current_game.display_version.c_str());
    gfx::drawTextArgs(this->vg, icon_x + icon_size + 30.f, icon_y + icon_size / 2 , 
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE, MOD_VERSION.c_str(), current_game.MOD_VERSION.c_str());

    if (current_game.MOD_VERSION == NONE_TYPE_TEXT || current_game.display_version == NONE_GAME_TEXT){
        gfx::drawTextArgs(this->vg, icon_x + icon_size + 30.f, icon_y + icon_size * 3 / 4 + 10.f, 
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE, VERSION_NONE.c_str());
    }else if (CompareModGameVersion(current_game.MOD_VERSION, current_game.display_version)){
        gfx::drawTextArgs(this->vg, icon_x + icon_size + 30.f, icon_y + icon_size * 3 / 4 + 10.f, 
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::TEAL, VERSION_OK.c_str());
    }else {
        gfx::drawTextArgs(this->vg, icon_x + icon_size + 30.f, icon_y + icon_size * 3 / 4 + 10.f, 
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::RED, VERSION_ERROR.c_str());
    }

    gfx::drawTextArgs(this->vg, icon_x, icon_y + icon_size + 40.f,
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::CYAN, MOD_CONTEXT.c_str());



    // 获取当前选中模组的信息 (Get current selected mod information)
    std::string MOD_TYPE_TEXT;

    MOD_TYPE_TEXT = GetModDescription(mod_info[this->mod_index].MOD_NAME, mod_info[this->mod_index].MOD_TYPE);

    if (MOD_TYPE_TEXT == ""){
        switch ((mod_info[this->mod_index].MOD_TYPE)[1]) {
            case 'F':
                MOD_TYPE_TEXT = FPS_TEXT;
                break;
            case 'G':
                MOD_TYPE_TEXT = HD_TEXT;
                break;
            case 'C':
                MOD_TYPE_TEXT = CHEAT_TEXT;
                break;
            case 'P':
                MOD_TYPE_TEXT = PLAY_TEXT;
                break;
            case 'B':
                MOD_TYPE_TEXT = COSMETIC_TEXT;
                break;
            default:
                MOD_TYPE_TEXT = NONE_TEXT;
                break;
        }
    }
        
        
    // 在右侧信息框绘制当前选中模组的详细信息（左对齐，顶部对齐）
    gfx::drawTextBoxCentered(this->vg, 
            icon_x ,                                 // X坐标
            icon_y + icon_size + 40.f + 14.f,             // Y坐标  
            sidebox_w - 70.f,                     // 宽度（减去左右边距）
            sidebox_h - icon_size - 35.f - 40.f - 21.f -22.f,         // 高度
            21.f,                                 // 字体大小
            1.5f,                                 // 行间距倍数
            MOD_TYPE_TEXT.c_str(),             // 当前选中模组的文本
            nullptr, 
            NVG_ALIGN_LEFT | NVG_ALIGN_TOP,       // 左对齐，顶部对齐
            gfx::Colour::WHITE
    );


    // 绘制左下角mod数量 (Draw mod count in bottom-left corner)
    gfx::drawTextArgs(this->vg, 55.f, 670.f, 24.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE, "%zu / %zu", this->mod_index + 1, mod_info.size());

    // 恢复NanoVG绘图状态 / Restore NanoVG drawing state
    nvgRestore(this->vg);

    gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_BACK.c_str()}, 
            gfx::pair{gfx::Button::A, BUTTON_SELECT.c_str()},
            gfx::pair{gfx::Button::X, CLEAR_BUTTON.c_str()}
        );

    // 绘制对话框（如果显示）(Draw dialog if shown)
    this->DrawDialog();

}

// 对比mod版本和游戏版本是否一致的辅助函数 (Helper function to compare mod version and game version consistency)
bool App::CompareModGameVersion(const std::string& mod_version, const std::string& game_version) {
    // 情况1：直接字符串匹配 (Case 1: Direct string match)
    if (mod_version == game_version) {
        return true;
    }
    
    // 情况2：简化版本号对比 - 去掉小数点和末尾0后对比 (Case 2: Simplified version comparison - remove dots and trailing zeros)
    auto simplify_version = [](const std::string& version) -> std::string {
        std::string simplified = version;
        
        // 移除所有空格 (Remove all spaces)
        simplified.erase(std::remove(simplified.begin(), simplified.end(), ' '), simplified.end());
        
        // 转换为小写 (Convert to lowercase)
        std::transform(simplified.begin(), simplified.end(), simplified.begin(), ::tolower);
        
        // 移除前缀v或V (Remove prefix v or V)
        if (!simplified.empty() && (simplified[0] == 'v')) {
            simplified = simplified.substr(1);
        }
        
        // 移除所有小数点 (Remove all dots)
        simplified.erase(std::remove(simplified.begin(), simplified.end(), '.'), simplified.end());
        
        // 移除末尾的所有0 (Remove all trailing zeros)
        while (!simplified.empty() && simplified.back() == '0') {
            simplified.pop_back();
        }
        
        // 如果结果为空，返回"0" (If result is empty, return "0")
        if (simplified.empty()) {
            simplified = "0";
        }
        
        return simplified;
    };
    
    std::string simple_mod = simplify_version(mod_version);
    std::string simple_game = simplify_version(game_version);
    
    // 简化后对比 (Compare after simplification)
    return simple_mod == simple_game;
}

void App::Sort()
{
    // std::scoped_lock lock{entries_mutex}; // 保护entries向量的排序操作 (Protect entries vector sorting operation)
    switch (static_cast<SortType>(this->sort_type)) {
        case SortType::Alphabetical_Reverse:
            // 按应用名称Z-A排序，但先按安装状态分组
            // Sort by app name Z-A, but group by installation status first
            std::ranges::sort(this->entries, [](const AppEntry& a, const AppEntry& b) {
                // 首先按安装状态分组：已安装的在前，未安装的在后
                // First group by installation status: installed first, uninstalled last
                bool a_installed = (a.display_version != NONE_GAME_TEXT);
                bool b_installed = (b.display_version != NONE_GAME_TEXT);
                
                if (a_installed != b_installed) {
                    return a_installed > b_installed; // 已安装的在前 (installed first)
                }
                
                // 在同一组内按字母顺序Z-A排序
                // Within the same group, sort alphabetically Z-A
                return a.FILE_NAME2 > b.FILE_NAME2;
            });
            break;
        case SortType::Alphabetical:
            // 按应用名称A-Z排序，但先按安装状态分组
            // Sort by app name A-Z, but group by installation status first
            std::ranges::sort(this->entries, [](const AppEntry& a, const AppEntry& b) {
                // 首先按安装状态分组：已安装的在前，未安装的在后
                // First group by installation status: installed first, uninstalled last
                bool a_installed = (a.display_version != NONE_GAME_TEXT);
                bool b_installed = (b.display_version != NONE_GAME_TEXT);
                
                if (a_installed != b_installed) {
                    return a_installed > b_installed; // 已安装的在前 (installed first)
                }
                
                // 在同一组内按字母顺序A-Z排序
                // Within the same group, sort alphabetically A-Z
                return a.FILE_NAME2 < b.FILE_NAME2;
            });
            break;
        default:
            // 默认按应用名称A-Z排序，但先按安装状态分组
            // Default sort by app name A-Z, but group by installation status first
            std::ranges::sort(this->entries, [](const AppEntry& a, const AppEntry& b) {
                // 首先按安装状态分组：已安装的在前，未安装的在后
                // First group by installation status: installed first, uninstalled last
                bool a_installed = (a.display_version != NONE_GAME_TEXT);
                bool b_installed = (b.display_version != NONE_GAME_TEXT);
                
                if (a_installed != b_installed) {
                    return a_installed > b_installed; // 已安装的在前 (installed first)
                }
                
                // 在同一组内按字母顺序A-Z排序
                // Within the same group, sort alphabetically A-Z
                return a.FILE_NAME2 < b.FILE_NAME2;
            });
            break;
    }
}

const char* App::GetSortStr() {
    switch (static_cast<SortType>(this->sort_type)) {
        case SortType::Alphabetical_Reverse:
            // 返回按字母顺序逆排序的提示文本
            return SORT_ALPHA_ZA.c_str();
        case SortType::Alphabetical:
            // 返回按字母顺序排序的提示文本
            return SORT_ALPHA_AZ.c_str();
        default:
            // 默认返回按容量从大到小排序的提示文本
            return SORT_ALPHA_AZ.c_str();
    }
}

void App::UpdateLoad() {
    if (this->controller.B) {
        this->async_thread.request_stop();
        this->async_thread.get();
        this->quit = true;
        return;
    }

    {
        std::scoped_lock lock{this->mutex};
        // 只要有应用加载完成就立即显示列表，实现真正的立即显示
        // Show list immediately when any app is loaded, achieving true immediate display
        if (scanned_count.load() > 0 || finished_scanning) {
            if (finished_scanning) {
                this->async_thread.get();
            }
            
            
            this->menu_mode = MenuMode::LIST;
        }
    }
}

void App::UpdateList() {
    // 如果对话框显示，优先处理对话框输入 (If dialog is shown, prioritize dialog input)
    if (this->show_dialog) {
        this->UpdateDialog();
        return; // 对话框显示时不处理列表输入 (Don't process list input when dialog is shown)
    }
    
    std::scoped_lock lock{entries_mutex}; // 保护entries向量的访问和修改 (Protect entries vector access and modification)
    // 检查应用列表是否为空，避免数组越界访问 (Check if application list is empty to avoid array out-of-bounds access)
    if (this->entries.empty()) {
        // 如果应用列表为空，只处理退出操作 (If application list is empty, only handle exit operation)
        if (this->controller.B) {
            this->audio_manager.PlayKeySound(0.9);
            this->quit = true;
        }
        return; // 提前返回，避免后续操作 (Early return to avoid subsequent operations)
    }
    
    // 扫描过程中禁用排序和删除功能
    if (this->controller.B) {
        this->audio_manager.PlayKeySound(0.9);
        this->quit = true;
    } else if (this->controller.A) { // 允许在扫描过程中选择列表项
        // 重置MOD列表的索引变量 (Reset MOD list index variables)
        this->mod_index = 0;
        this->mod_start = 0;
        this->mod_yoff = 130.f;
        this->mod_ypos = 130.f;

        if (!mod_icons_loaded) {
            // 加载所有MOD图标 (Load all MOD icons)
            this->Cheat_code_MOD_image = nvgCreateImage(this->vg, "romfs:/Cheat_code_MOD.jpg", NVG_IMAGE_NEAREST);
            this->Cosmetic_MOD_image = nvgCreateImage(this->vg, "romfs:/Cosmetic_MOD.jpg", NVG_IMAGE_NEAREST);
            this->FPS_MOD_image = nvgCreateImage(this->vg, "romfs:/FPS_MOD.jpg", NVG_IMAGE_NEAREST);
            this->HD_MOD_image = nvgCreateImage(this->vg, "romfs:/HD_MOD.jpg", NVG_IMAGE_NEAREST);
            this->More_PLAY_MOD_image = nvgCreateImage(this->vg, "romfs:/More_PLAY_MOD.jpg", NVG_IMAGE_NEAREST);
            this->NONE_MOD_image = nvgCreateImage(this->vg, "romfs:/NONE_MOD.jpg", NVG_IMAGE_NEAREST);

            mod_icons_loaded = true; // 标记为已加载 (Mark as loaded)
        }
    
        // 智能缓存机制：只有当游戏发生变化时才重新加载映射数据 (Smart cache mechanism: only reload mapping data when game changes)
        const auto& current_game = this->entries[this->index];
        if (current_game_name != current_game.FILE_NAME) {
            LoadModNameMapping(current_game.FILE_NAME, current_game.FILE_PATH);
        }
        FastScanModInfo();
        Sort_Mod();
        this->audio_manager.PlayKeySound(0.9);
        this->menu_mode = MenuMode::MODLIST;
    } else if (this->controller.X) { // X键显示确认对话框 (X key to show confirmation dialog)
        
        this->ShowDialog(DialogType::INFO,
             "作者：TOM\n版本：1.0.0\nQ群：1051287661\n乡村大舞台，折腾你就来！",
              26.0f,NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);

        this->audio_manager.PlayConfirmSound(); // 播放确认音效 (Play confirm sound effect)
        
    } else if (this->controller.START) { // +键显示使用说明界面
        
        this->audio_manager.PlayKeySound(0.9);
        this->menu_mode = MenuMode::INSTRUCTION;
        
    } else if (this->controller.DOWN) { // move down
        if (this->index < (this->entries.size() - 1)) {
            this->audio_manager.PlayKeySound(0.9);
            this->index++;
            this->ypos += this->BOX_HEIGHT;
            if ((this->ypos + this->BOX_HEIGHT) > 646.f) {
                LOG("moved down\n");
                this->ypos -= this->BOX_HEIGHT;
                this->yoff = this->ypos - ((this->index - this->start - 1) * this->BOX_HEIGHT);
                this->start++;
            }
            // 光标移动后的图标加载由每帧调用自动处理
            // Icon loading after cursor movement is handled by per-frame calls
        }else this->audio_manager.PlayLimitSound(1.5); 
    } else if (this->controller.UP) { // move up
        if (this->index != 0 && this->entries.size()) {
            this->audio_manager.PlayKeySound(0.9);
            this->index--;
            this->ypos -= this->BOX_HEIGHT;
            if (this->ypos < 86.f) {
                LOG("moved up\n");
                this->ypos += this->BOX_HEIGHT;
                this->yoff = this->ypos;
                this->start--;
            }// 播放按键音效 (Play key sound)
            // 光标移动后触发视口感知图标加载
            // 光标移动后的图标加载由每帧调用自动处理
            // Icon loading after cursor movement is handled by per-frame calls
        }else this->audio_manager.PlayLimitSound(1.5); 
    } else if (!is_scan_running && this->controller.Y) { // 非扫描状态下才允许排序
        this->audio_manager.PlayKeySound(); // 播放按键音效 (Play key sound)
        this->sort_type++;

        if (this->sort_type == std::to_underlying(SortType::MAX)) {
            this->sort_type = 0;
        }

        this->Sort();
        
        // 强制重置可见区域缓存，确保排序后图标能重新加载 (Force reset visible range cache to ensure icons reload after sorting)
        this->last_loaded_range = {SIZE_MAX, SIZE_MAX};
        
        // 重置选择索引，使选择框回到第一项
        this->index = 0;
        // 重置滚动位置
        this->ypos = this->yoff = 130.f;
        this->start = 0;

    } else if (this->controller.L) { // L键向上翻页 (L key for page up)
        if (this->entries.size() > 0) {
            // 直接更新index，向上翻页4个位置 (Directly update index, page up 4 positions)
            if (this->index >= 4) {
                this->audio_manager.PlayKeySound(0.9); // 正常翻页音效 (Normal page flip sound)
                this->index -= 4;
            } else {
                this->index = 0;
                this->audio_manager.PlayLimitSound(1.5);
            }
            
            // 直接更新start位置，确保翻页显示4个项目 (Directly update start position to ensure 4 items per page)
            if (this->start >= 4) {
                this->start -= 4;
            } else {
                this->start = 0;
            }
            
            // 边界检查：确保index和start的一致性 (Boundary check: ensure consistency between index and start)
            if (this->index < this->start) {
                this->index = this->start;
            }
            
            // 边界检查：确保index不会超出当前页面范围 (Boundary check: ensure index doesn't exceed current page range)
            std::size_t max_index_in_page = this->start + 3; // 当前页面最大索引 (Maximum index in current page)
            if (this->index > max_index_in_page && max_index_in_page < this->entries.size()) {
                this->index = max_index_in_page;
            }
            
            // 更新相关显示变量 (Update related display variables)
            this->ypos = 130.f + (this->index - this->start) * this->BOX_HEIGHT;
            this->yoff = 130.f;
            
            // 翻页后的图标加载由每帧调用自动处理 (Icon loading after page change is handled by per-frame calls)
        }
    } else if (this->controller.R) { // R键向下翻页 (R key for page down)
        if (this->entries.size() > 0) {
            // 直接更新index，向下翻页4个位置 (Directly update index, page down 4 positions)
            this->index += 4;
            
            // 边界检查：确保不超出列表范围 (Boundary check: ensure not exceeding list range)
            if (this->index >= this->entries.size()) {
                this->index = this->entries.size() - 1;
                this->audio_manager.PlayLimitSound(1.5);
            }else this->audio_manager.PlayKeySound(0.9);
            
            // 直接更新start位置，确保翻页显示4个项目 (Directly update start position to ensure 4 items per page)
            this->start += 4;
            
            // 边界检查：确保start不会超出合理范围 (Boundary check: ensure start doesn't exceed reasonable range)
            if (this->entries.size() > 4) {
                std::size_t max_start = this->entries.size() - 4;
                if (this->start > max_start) {
                    this->start = max_start;
                    // 当到达末尾时，调整index到最后一个可见项 (When reaching end, adjust index to last visible item)
                    this->index = this->entries.size() - 1;
                }
            } else {
                this->start = 0;
            }
            
            // 更新相关显示变量 (Update related display variables)
            this->ypos = 130.f + (this->index - this->start) * this->BOX_HEIGHT;
            this->yoff = 130.f;
            
            // 翻页后的图标加载由每帧调用自动处理 (Icon loading after page change is handled by per-frame calls)
        }
    } 

    
    // handle direction keys
}

void App::UpdateModList() {
    // 如果对话框显示，优先处理对话框输入 (If dialog is shown, prioritize dialog input)
    if (this->show_dialog) {
        this->UpdateDialog();
        return; // 对话框显示时不处理列表输入 (Don't process list input when dialog is shown)
    }
    
    std::string select_mod_name;
    bool select_mod_state;



    // 如果当前游戏没有MOD，只处理返回操作 (If current game has no MODs, only handle back operation)
    if (mod_info.empty()) {
        if (this->controller.B) {
            this->audio_manager.PlayKeySound(0.9);
            this->menu_mode = MenuMode::LIST;
        }
        return;
    }else {
        
        select_mod_name = this->mod_info[this->mod_index].MOD_NAME2;
        select_mod_state = this->mod_info[this->mod_index].MOD_STATE;

    }
    
    if (this->controller.B) {

        this->audio_manager.PlayKeySound(0.9);
        this->menu_mode = MenuMode::LIST;
    } else if (this->controller.A) {

        if (select_mod_state) this->ShowDialog(DialogType::CONFIRM,GetSnprintf(CONFIRM_UNINSTALLED,select_mod_name),26.0f,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
        else this->ShowDialog(DialogType::CONFIRM,GetSnprintf(CONFIRM_INSTALLED,select_mod_name),26.0f,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
        

        this->audio_manager.PlayConfirmSound();

    } else if (this->controller.X) { // X键清理缓存 (X key to clear cache)
        
        this->audio_manager.PlayConfirmSound();
        this->clean_button = true;
        this->ShowDialog(DialogType::CONFIRM,GetSnprintf(CONFIRM_UNINSTALLED,select_mod_name),26.0f,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);

    } else if (this->controller.DOWN) { // 向下移动 (Move down)
        if (this->mod_index < (mod_info.size() - 1)) {
            this->audio_manager.PlayKeySound(0.9);
            this->mod_index++;
            this->mod_ypos += this->BOX_HEIGHT;
            if ((this->mod_ypos + this->BOX_HEIGHT) > 646.f) {
                this->mod_ypos -= this->BOX_HEIGHT;
                this->mod_yoff = this->mod_ypos - ((this->mod_index - this->mod_start - 1) * this->BOX_HEIGHT);
                this->mod_start++;
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.UP) { // 向上移动 (Move up)
        if (this->mod_index != 0 && mod_info.size()) {
            this->audio_manager.PlayKeySound(0.9);
            this->mod_index--;
            this->mod_ypos -= this->BOX_HEIGHT;
            if (this->mod_ypos < 86.f) {
                this->mod_ypos += this->BOX_HEIGHT;
                this->mod_yoff = this->mod_ypos;
                this->mod_start--;
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.L) { // L键向上翻页 (L key for page up)
        if (mod_info.size() > 0) {
            // 直接更新index，向上翻页4个位置 (Directly update index, page up 4 positions)
            if (this->mod_index >= 4) {
                this->audio_manager.PlayKeySound(0.9); // 正常翻页音效 (Normal page flip sound)
                this->mod_index -= 4;
            } else {
                this->mod_index = 0;
                this->audio_manager.PlayLimitSound(1.5);
            }
            
            // 直接更新mod_start位置，确保翻页显示4个项目 (Directly update mod_start position to ensure 4 items per page)
            if (this->mod_start >= 4) {
                this->mod_start -= 4;
            } else {
                this->mod_start = 0;
            }
            
            // 边界检查：确保mod_index和mod_start的一致性 (Boundary check: ensure consistency between mod_index and mod_start)
            if (this->mod_index < this->mod_start) {
                this->mod_index = this->mod_start;
            }
            
            // 边界检查：确保index不会超出当前页面范围 (Boundary check: ensure index doesn't exceed current page range)
            std::size_t max_index_in_page = this->mod_start + 3; // 当前页面最大索引 (Maximum index in current page)
            if (this->mod_index > max_index_in_page && max_index_in_page < mod_info.size()) {
                this->mod_index = max_index_in_page;
            }
            
            // 更新相关显示变量 (Update related display variables)
            this->mod_ypos = 130.f + (this->mod_index - this->mod_start) * this->BOX_HEIGHT;
            this->mod_yoff = 130.f;
        }
    } else if (this->controller.R) { // R键向下翻页 (R key for page down)
        if (mod_info.size() > 0) {
            // 直接更新mod_index，向下翻页4个位置 (Directly update mod_index, page down 4 positions)
            this->mod_index += 4;
            
            // 边界检查：确保不超出列表范围 (Boundary check: ensure not exceeding list range)
            if (this->mod_index >= mod_info.size()) {
                this->mod_index = mod_info.size() - 1;
                this->audio_manager.PlayLimitSound(1.5);
            } else {
                this->audio_manager.PlayKeySound(0.9);
            }
            
            // 直接更新mod_start位置，确保翻页显示4个项目 (Directly update mod_start position to ensure 4 items per page)
            this->mod_start += 4;
            
            // 边界检查：确保mod_start不会超出合理范围 (Boundary check: ensure mod_start doesn't exceed reasonable range)
            if (mod_info.size() > 4) {
                std::size_t max_start = mod_info.size() - 4;
                if (this->mod_start > max_start) {
                    this->mod_start = max_start;
                    // 当到达末尾时，调整mod_index到最后一个可见项 (When reaching end, adjust mod_index to last visible item)
                    this->mod_index = mod_info.size() - 1;
                }
            } else {
                this->mod_start = 0;
            }
            
            // 更新相关显示变量 (Update related display variables)
            this->mod_ypos = 130.f + (this->mod_index - this->mod_start) * this->BOX_HEIGHT;
            this->mod_yoff = 130.f;
        }
    }
}


// 快速获取应用基本信息并缓存图标数据
// Fast get application basic info and cache icon data
bool App::TryGetAppBasicInfoWithIconCache(u64 application_id, AppEntry& entry) {
    
    // 1. 优先尝试从缓存获取应用名称
    // 1. Try to get application name from cache first
    NxTitleCacheApplicationMetadata* cached_metadata = nxtcGetApplicationMetadataEntryById(application_id);
    if (cached_metadata != nullptr) {
        entry.name = cached_metadata->name;
        entry.id = cached_metadata->title_id;
        entry.display_version = cached_metadata->version; 
        // 暂时设置默认值，稍后异步加载
        // Set default values temporarily, load asynchronously later
        entry.image = this->default_icon_image;
        entry.own_image = false;
        
        // 缓存图标数据到AppEntry中
        // Cache icon data in AppEntry
        if (cached_metadata->icon_data && cached_metadata->icon_size > 0) {
            entry.cached_icon_data.resize(cached_metadata->icon_size);
            memcpy(entry.cached_icon_data.data(), cached_metadata->icon_data, cached_metadata->icon_size);
            entry.has_cached_icon = true;
        } else {
            entry.has_cached_icon = false;
        }
        
        nxtcFreeApplicationMetadata(&cached_metadata);
        
        return true;
    }
    
    // 2. 缓存获取失败，尝试从NS服务获取（仅名称）
    // 2. Cache failed, try to get from NS service (name only)
    auto control_data = std::make_unique<NsApplicationControlData>();
    u64 jpeg_size{};
    
    Result result = nsGetApplicationControlData(NsApplicationControlSource_Storage, application_id, control_data.get(), sizeof(NsApplicationControlData), &jpeg_size);
    if (R_FAILED(result)) {
        return false;
    }
    
    int systemlanguage = tj::LangManager::getInstance().getCurrentLanguage();
    NacpLanguageEntry* language_entry = &control_data->nacp.lang[systemlanguage];
    
    // 如果当前语言条目为空，则遍历查找第一个非空条目
    // If the current language entry is empty, iterate to find the first non-empty entry
    if (language_entry->name[0] == '\0' && language_entry->author[0] == '\0') {
        for (int i = 0; i < 16; i++) {
            if (control_data->nacp.lang[i].name[0] != '\0' || control_data->nacp.lang[i].author[0] != '\0') {
                language_entry = &control_data->nacp.lang[i];
                break;
            }
        }
    }
    
    entry.name = language_entry->name;
    entry.id = application_id;
    entry.display_version = control_data->nacp.display_version; // 从NACP获取显示版本 (Get display version from NACP)
    // 暂时设置默认值，稍后异步加载
    // Set default values temporarily, load asynchronously later
    entry.image = this->default_icon_image;
    entry.own_image = false;
    
    // 缓存图标数据到AppEntry中，避免后续重复读取
    // Cache icon data in AppEntry to avoid repeated reads later
    if (jpeg_size > sizeof(NacpStruct)) {
        size_t icon_size = jpeg_size - sizeof(NacpStruct);
        entry.cached_icon_data.resize(icon_size);
        memcpy(entry.cached_icon_data.data(), control_data->icon, icon_size);
        entry.has_cached_icon = true;
        
        // 仍然添加到缓存系统以供其他用途
        // Still add to cache system for other uses
        nxtcAddEntry(application_id, &control_data->nacp, icon_size, control_data->icon, true);
    } else {
        entry.has_cached_icon = false;
    }
    
    return true;
}



// 辅助函数：根据filename_path获取其下面的modid和mod目录数量（使用标准C库）
// Helper function: Get modid and mod directory count under filename_path (using standard C library)
std::pair<u64, std::string> App::GetModIdAndSubdirCountStdio(const std::string& filename_path) {
    // 打开filename目录
    // Open filename directory
    DIR* filename_dir = opendir(filename_path.c_str());
    if (!filename_dir) {
        return {0, "0"}; // 返回空结果
    }
    
    // 读取第一个有效目录条目（应该是唯一的modid文件夹）
    // Read the first valid directory entry (should be the only modid folder)
    struct dirent* entry;
    u64 application_id = 0;
    std::string modid_name; // 保存modid目录名，避免在closedir后访问失效指针
    
    while ((entry = readdir(filename_dir)) != nullptr) {
        // 跳过当前目录和父目录
        // Skip current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 将modid字符串转换为u64类型的应用ID
        // Convert modid string to u64 application ID
        char* endptr;
        application_id = strtoull(entry->d_name, &endptr, 16);
        
        // 检查转换是否成功（modid应该是16进制字符串）
        // Check if conversion was successful (modid should be hex string)
        if (*endptr == '\0' && application_id != 0) {
            modid_name = entry->d_name; // 保存有效的modid目录名
            break;
        }
    }
    
    closedir(filename_dir);
    
    if (application_id == 0 || modid_name.empty()) {
        return {0, "0"}; // 未找到有效的modid
    }
    
    // 读取modid目录下的子目录数量
    // Read subdirectory count under modid directory
    std::string modid_path = filename_path + "/" + modid_name;
    DIR* modid_dir = opendir(modid_path.c_str());
    
    int mod_count = 0; // MOD目录数量计数器
    if (modid_dir) {
        struct dirent* subentry;
        while ((subentry = readdir(modid_dir)) != nullptr) {
            // 跳过当前目录和父目录和带点的文件
            // Skip current and parent directory entries and files with dots
            std::string dirname = subentry->d_name;
            if (strcmp(subentry->d_name, ".") == 0 || strcmp(subentry->d_name, "..") == 0 ||(dirname.find('.') != std::string::npos)) {
                continue;
            }
            
            mod_count++; // 计数有效的MOD目录
        }
        closedir(modid_dir);
    }
    
    return {application_id, std::to_string(mod_count)};
}





void App::FastScanModInfo() {
    // 清空之前的MOD信息
    // Clear previous MOD information
    this->mod_info.clear();
    
    // 获取当前选中游戏的文件路径和MOD版本（加锁保护读取操作）
    // Get filepath and MOD version of currently selected game (with lock protection for read operation)
   
    
    std::string mod_path = this->entries[this->index].FILE_PATH;
    // 打开MOD目录
    // Open MOD directory
    DIR* mod_dir = opendir(mod_path.c_str());
    if (!mod_dir) {
        return; // 无法打开目录，直接返回 (Cannot open directory, return directly)
    }

    struct dirent* entry;
    // 遍历MOD目录下的所有条目
    // Iterate through all entries in MOD directory
    while ((entry = readdir(mod_dir)) != nullptr) {
        // 跳过当前目录和父目录
        // Skip current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 跳过带点的文件
        // Skip files with dots
        std::string dirname = entry->d_name;
        if (dirname.find('.') != std::string::npos) {
            continue;
        }
        
        // 创建MODINFO结构体并解析目录名称
        // Create MODINFO structure and parse directory name
        MODINFO mod_info_item;

        mod_info_item.MOD_PATH = mod_path + "/" + dirname;
        
        // 检查末尾是否有$符号来判断安装状态
        // Check if there's a $ symbol at the end to determine installation status
        if (!dirname.empty() && dirname.back() == '$') {
            mod_info_item.MOD_STATE = true;
            dirname.pop_back(); // 移除末尾的$符号 (Remove the $ symbol at the end)
        } else {
            mod_info_item.MOD_STATE = false; // 设置安装状态 (Set installation state)
        }
        
        // 解析目录名称格式：XXXX[X] 或 XXXX
        // Parse directory name format: XXXX[X] or XXXX
        size_t bracket_start = dirname.find('[');
        size_t bracket_end = dirname.find(']');
        
        if (bracket_start != std::string::npos && bracket_end != std::string::npos && bracket_end > bracket_start) {
            // XXXX[X] 格式：XXXX存储到MOD_NAME，[X]存储到MOD_TYPE
            // XXXX[X] format: XXXX to MOD_NAME, [X] to MOD_TYPE
            mod_info_item.MOD_NAME = dirname.substr(0, bracket_start);
            mod_info_item.MOD_TYPE = dirname.substr(bracket_start, bracket_end - bracket_start + 1);
        } else {
            // XXXX 格式：XXXX存储到MOD_NAME，MOD_TYPE为空
            // XXXX format: XXXX to MOD_NAME, MOD_TYPE empty
            mod_info_item.MOD_NAME = dirname;
            mod_info_item.MOD_TYPE = NONE_TYPE_TEXT;
        }
        mod_info_item.MOD_NAME2 = GetMappedModName(mod_info_item.MOD_NAME, mod_info_item.MOD_TYPE);
        // 将解析的MOD信息添加到容器中
        // Add parsed MOD information to container
        this->mod_info.push_back(mod_info_item);
    }
    
    // 关闭目录
    // Close directory
    closedir(mod_dir);
}



// 分离式扫描：第一阶段快速扫描应用名称和大小
// Separated scanning: Phase 1 - Fast scan application names
void App::FastScanNames(std::stop_token stop_token) {
    // 标记扫描开始
    is_scan_running = true;
    // 初始化扫描计数器
    scanned_count = 0;

    // 计数器，用于统计找到的应用总数
    size_t count{};

    // 初始化libnxtc库
    if (!nxtcInitialize()) {
        LOG("初始化libnxtc库失败\n");
    }

    // SD卡会自动挂载，直接使用标准路径
    // SD card is automatically mounted, use standard paths directly
    DIR* mods_dir = opendir("/mods2");
    if (!mods_dir) {
        LOG("无法打开MODS2路径: /mods2\n");
        closedir(mods_dir);
        goto done;
    }

    struct dirent* filename_entry;
    // 遍历mods2下的filename文件夹
    // Iterate through filename folders under mods2
    while ((filename_entry = readdir(mods_dir)) != nullptr) {
        if (stop_token.stop_requested()) {
            closedir(mods_dir);
            break;
        }
        
        // 跳过当前目录和父目录
        // Skip current and parent directory entries
        if (strcmp(filename_entry->d_name, ".") == 0 || strcmp(filename_entry->d_name, "..") == 0) {
            continue;
        }
        
        // 分割文件名，提取应用名称和版本号
        // Split filename to extract app name and version
        std::string full_name = filename_entry->d_name;
        std::string FILE_NAME;
        std::string MOD_VERSION;
        
        // 查找方括号的位置
        // Find bracket positions
        size_t bracket_start = full_name.find('[');
        size_t bracket_end = full_name.find(']');
        
        if (bracket_start != std::string::npos && bracket_end != std::string::npos && bracket_end > bracket_start) {
            // 格式: XXXX[1.0.2]
            // Format: XXXX[1.0.2]
            FILE_NAME = full_name.substr(0, bracket_start);
            MOD_VERSION = full_name.substr(bracket_start + 1, bracket_end - bracket_start - 1);
        } else {
            // 格式: XXXX
            // Format: XXXX
            FILE_NAME = full_name;
            MOD_VERSION = NONE_TYPE_TEXT;
        }
        // 构建filename文件夹的完整路径
        // Build full path for filename folder
        std::string filename_path = "/mods2/" + std::string(filename_entry->d_name);
        
        // 获取当前filename目录下的modid和mod目录数量
        // Get modid and mod directory count under current filename directory
        std::pair<u64, std::string> modid_count_result = GetModIdAndSubdirCountStdio(filename_path);
        
        // 从返回的pair中提取application_id和mod目录数量字符串
        // Extract application_id and mod directory count string from returned pair
        u64 application_id = modid_count_result.first;
        std::string mod_count = modid_count_result.second;
        
        // 如果没有找到有效的modid，跳过当前目录
        // Skip current directory if no valid modid found
        if (application_id == 0 || mod_count == "0") {
            continue;
        }

        char app_id[17]; // 16位十六进制字符串 + 结束符 (16-digit hex string + null terminator)
        snprintf(app_id, sizeof(app_id), "%016lX", application_id);

        // 应用条目，用于存储单个应用的信息
        AppEntry entry;
        bool is_corrupted = false;

        

        // 1. 快速获取应用名称和大小信息（不包含图标）
        // 1. Fast get application name and size info (excluding icon)
        if (!TryGetAppBasicInfoWithIconCache(application_id, entry)) {
            is_corrupted = true;
        }else{
            entry.FILE_NAME = FILE_NAME;
            entry.FILE_PATH = filename_path + "/" + app_id;
            entry.MOD_TOTAL = mod_count;
            entry.MOD_VERSION = MOD_VERSION;
            entry.FILE_NAME2 = GetMappedGameName(FILE_NAME, MOD_VERSION);
        }
        
        // 标记是否存在损坏的安装
        // Mark if there is a corrupted installation
        if (is_corrupted) {
            // 损坏的安装处理
            // Handle corrupted installation
            entry.name = FILE_NAME;
            entry.FILE_NAME = FILE_NAME;
            entry.FILE_PATH = filename_path + "/" + app_id;
            entry.display_version = NONE_GAME_TEXT;
            entry.id = application_id;
            entry.image = this->default_icon_image;
            entry.own_image = false;
            // 损坏安装的模组数量显示为--
            // Show mod count as -- for corrupted installations
            entry.MOD_TOTAL = mod_count;
            entry.MOD_VERSION = MOD_VERSION;
            entry.FILE_NAME2 = GetMappedGameName(FILE_NAME, MOD_VERSION);
        }

        // 加锁保护entries列表
        {
            std::scoped_lock lock{entries_mutex};
            // 将应用条目添加到列表
            this->entries.emplace_back(std::move(entry));
            // 更新扫描计数器
            scanned_count++;
            count++;
        }

        // 为首屏4个应用立即提交最高优先级图标加载任务
        // Immediately submit highest priority icon loading tasks for first screen 4 apps
        if (count <= BATCH_SIZE && !is_corrupted && entry.has_cached_icon) {
            ResourceLoadTask icon_task;
            icon_task.application_id = application_id;
            icon_task.priority = 0; // 最高优先级 (Highest priority)
            icon_task.submit_time = std::chrono::steady_clock::now();
            icon_task.task_type = ResourceTaskType::ICON;
            
            icon_task.load_callback = [this, application_id]() {
                std::vector<unsigned char> icon_data;
                bool has_icon_data = false;
                
                // 获取缓存的图标数据
                // Get cached icon data
                {
                    std::scoped_lock lock{entries_mutex};
                    auto it = std::find_if(entries.begin(), entries.end(),
                        [application_id](const AppEntry& entry) {
                            return entry.id == application_id && entry.has_cached_icon;
                        });
                    
                    if (it != entries.end()) {
                        icon_data = it->cached_icon_data;
                        has_icon_data = true;
                    }
                }
                
                // 验证并创建图像
                // Validate and create image
                if (has_icon_data && !icon_data.empty() && IsValidJpegData(icon_data)) {
                    int image_id = nvgCreateImageMem(this->vg, 0, icon_data.data(), icon_data.size());
                    if (image_id > 0) {
                        std::scoped_lock lock{entries_mutex};
                        auto it = std::find_if(entries.begin(), entries.end(),
                            [application_id](const AppEntry& entry) {
                                return entry.id == application_id;
                            });
                        
                        if (it != entries.end()) {
                            // 如果之前有自己的图像，先删除
                            // If previously had own image, delete it first
                            if (it->own_image && it->image != this->default_icon_image) {
                                nvgDeleteImage(this->vg, it->image);
                            }
                            it->image = image_id;
                            it->own_image = true;
                        }
                    }
                }
            };
            
            resource_manager.submitLoadTask(icon_task);
        }

        // 首批应用名称加载完成标记
        // Mark first batch names loading complete
        if (scanned_count.load() == BATCH_SIZE) {
            initial_batch_loaded = true;
        }

        //线程休息1ms避免卡死（比原来更快）
        svcSleepThread(1000000ULL);
    }
    
    // 清理资源
    // Clean up resources
    closedir(mods_dir);

done:
    // 标记扫描结束
    is_scan_running = false;

    // 刷新缓存文件
    nxtcFlushCacheFile();

    // 退出libnxtc库
    nxtcExit();

    // 加锁保护finished_scanning
    std::scoped_lock lock{this->mutex};

    // 标记扫描完成
    this->finished_scanning = true;
}


// 计算当前可见区域的应用索引范围
// Calculate the index range of applications in current visible area
std::pair<size_t, size_t> App::GetVisibleRange() const {
    std::scoped_lock lock{entries_mutex};
    
    if (entries.empty()) {
        return {0, 0};
    }
    
    // 屏幕上最多显示4个应用项
    // Maximum 4 application items can be displayed on screen
    constexpr size_t MAX_VISIBLE_ITEMS = 4;
    
    size_t visible_start = this->start;
    size_t visible_end = std::min(visible_start + MAX_VISIBLE_ITEMS, entries.size());
    
    return {visible_start, visible_end};
}

// 获取列表界面的可见范围 (Get visible range for list interface)


// 基于视口的智能图标加载：根据光标位置优先加载可见区域的图标
// Viewport-aware smart icon loading: prioritize loading icons in visible area based on cursor position

void App::LoadVisibleAreaIcons() {
    // 如果应用列表为空，直接返回，避免没有应用的设备出现问题
    // If application list is empty, return directly to avoid issues on devices without apps
    {
        std::scoped_lock lock{entries_mutex};
        if (entries.empty()) {
            return;
        }
    }

    auto now = std::chrono::steady_clock::now();
    
    // 防抖：如果距离上次调用不足100ms，则跳过
    // Debouncing: skip if less than 100ms since last call
    constexpr auto DEBOUNCE_INTERVAL = std::chrono::milliseconds(100);
    if (now - last_load_time < DEBOUNCE_INTERVAL) {
        return;
    }
    last_load_time = now;
    
    auto [visible_start, visible_end] = GetVisibleRange();
    
    // 如果可见区域没有变化且不是强制重置状态，则跳过
    // Skip if visible range hasn't changed and not in force reset state
    if (last_loaded_range.first == visible_start && last_loaded_range.second == visible_end && 
        last_loaded_range.first != SIZE_MAX) {
        return;
    }
    last_loaded_range = {visible_start, visible_end};
    
    // 图标加载策略：加载当前屏幕4个应用和下方即将进入屏幕的应用
    // Icon loading strategy: load current screen 4 apps and upcoming apps below
    constexpr size_t PRELOAD_BUFFER = 2; // 下方预加载的应用数量
    constexpr size_t FIRST_SCREEN_SIZE = 4; // 首屏应用数量
    
    size_t load_start = visible_start; // 从当前可见区域开始，不预加载上方
    size_t load_end;
    {
        std::scoped_lock lock{entries_mutex};
        load_end = std::min(visible_end + PRELOAD_BUFFER, entries.size());
    }
    
    // 批量收集需要加载图标的应用信息，减少锁的使用
    // Batch collect applications that need icon loading to reduce lock usage
    struct LoadInfo {
        u64 application_id;
        int priority;
    };
    
    std::vector<LoadInfo> load_infos;
    load_infos.reserve(load_end - load_start);
    
    {
        std::scoped_lock lock{entries_mutex};
        // 确保索引范围有效
        // Ensure index range is valid
        const size_t actual_end = std::min(load_end, entries.size());
        
        for (size_t i = load_start; i < actual_end; ++i) {
            const auto& entry = entries[i];
            
            // 优化：同时检查图标状态和损坏状态，减少后续处理
            // Optimization: check both icon status and corruption status to reduce subsequent processing
            if (entry.image == this->default_icon_image && entry.display_version != NONE_GAME_TEXT) {
                LoadInfo info;
                info.application_id = entry.id;
                
                // 优先级策略：首屏4个应用最高优先级(0)，当前可见区域次高优先级(1)，其他为低优先级(2)
                // Priority strategy: first screen 4 apps highest priority(0), current visible area medium priority(1), others low priority(2)
                if (i < FIRST_SCREEN_SIZE) {
                    info.priority = 0; // 首屏应用最高优先级
                } else if (i >= visible_start && i < visible_end) {
                    info.priority = 1; // 当前可见区域次高优先级
                } else {
                    info.priority = 2; // 其他应用低优先级
                }
                
                load_infos.push_back(std::move(info));
            }
        }
    }
    
    // 如果没有需要加载的图标，直接返回
    // Return early if no icons need loading
    if (load_infos.empty()) {
        return;
    }
    
    // 处理收集到的加载信息
    // Process collected loading information
    for (const auto& info : load_infos) {
        
        // 提交图标加载任务
        // Submit icon loading task
        ResourceLoadTask icon_task;
        icon_task.application_id = info.application_id;
        icon_task.priority = info.priority;
        icon_task.submit_time = std::chrono::steady_clock::now();
        icon_task.task_type = ResourceTaskType::ICON; // 标记为图标任务 (Mark as icon task)
        
        icon_task.load_callback = [this, application_id = info.application_id]() {
            // 获取图标数据
            // Get icon data
            std::vector<unsigned char> icon_data;
            bool has_icon_data = false;
            
            // 优先使用AppEntry中缓存的图标数据，避免重复的缓存读取
            // Prioritize using cached icon data in AppEntry to avoid repeated cache reads
            {
                std::scoped_lock lock{entries_mutex};
                auto it = std::find_if(entries.begin(), entries.end(),
                    [application_id](const AppEntry& entry) {
                        return entry.id == application_id && entry.has_cached_icon;
                    });
                
                if (it != entries.end()) {
                    icon_data = it->cached_icon_data;
                    has_icon_data = true;
                }
            }
            
            
             if (!has_icon_data) {
                 return; // 没有可用的图标数据，直接返回
             }
            
            // 验证并创建图像
            // Validate and create image
            if (has_icon_data && !icon_data.empty() && IsValidJpegData(icon_data)) {
                int image_id = nvgCreateImageMem(this->vg, 0, icon_data.data(), icon_data.size());
                if (image_id > 0) {
                    std::scoped_lock lock{entries_mutex};
                    auto it = std::find_if(entries.begin(), entries.end(),
                        [application_id](const AppEntry& entry) {
                            return entry.id == application_id;
                        });
                    
                    if (it != entries.end()) {
                        // 如果之前有自己的图像，先删除
                        // If previously had own image, delete it first
                        if (it->own_image && it->image != this->default_icon_image) {
                            nvgDeleteImage(this->vg, it->image);
                        }
                        it->image = image_id;
                        it->own_image = true;
                    }
                }
            }
        };
        
        this->resource_manager.submitLoadTask(icon_task);
    }
}

// 卸载界面的可见区域图标加载 (Visible area icon loading for uninstall interface)
// 只加载当前屏幕可见的应用图标和下方2个预加载图标，避免内存浪费
// Only load icons for currently visible apps and 2 preload icons below, avoiding memory waste


// 加载卸载界面的图标 (Load icons for uninstall interface)
// 为列表界面中的应用加载图标 (Load icons for apps in list interface)


App::App() {
    // 使用封装后的方法自动加载系统语言
    // Automatically load the system language using lang_manager
    tj::LangManager::getInstance().loadSystemLanguage();
    
    
    
    PlFontData font_standard, font_extended, font_lang;
    
    // 加载默认的字体，用于显示拉丁文 
    // Load the default font for displaying Latin characters
    plGetSharedFontByType(&font_standard, PlSharedFontType_Standard);
    plGetSharedFontByType(&font_extended, PlSharedFontType_NintendoExt);

    

    // Create the deko3d device
    this->device = dk::DeviceMaker{}.create();

    // Create the main queue
    this->queue = dk::QueueMaker{this->device}.setFlags(DkQueueFlags_Graphics).create();

    // Create the memory pools
    this->pool_images.emplace(device, DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, 16*1024*1024);
    this->pool_code.emplace(device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code, 128*1024);
    this->pool_data.emplace(device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, 1*1024*1024);

    // Create the static command buffer and feed it freshly allocated memory
    this->cmdbuf = dk::CmdBufMaker{this->device}.create();
    const CMemPool::Handle cmdmem = this->pool_data->allocate(this->StaticCmdSize);
    this->cmdbuf.addMemory(cmdmem.getMemBlock(), cmdmem.getOffset(), cmdmem.getSize());

    // Create the framebuffer resources
    this->createFramebufferResources();
    
    // 初始化动态命令缓冲区用于GPU命令优化
    // Initialize dynamic command buffers for GPU command optimization
    for (unsigned i = 0; i < NumCommandBuffers; ++i) {
        this->dynamic_cmdbufs[i] = dk::CmdBufMaker{this->device}.create();
        const CMemPool::Handle dynamic_cmdmem = this->pool_data->allocate(this->StaticCmdSize);
        this->dynamic_cmdbufs[i].addMemory(dynamic_cmdmem.getMemBlock(), dynamic_cmdmem.getOffset(), dynamic_cmdmem.getSize());
        
        // 初始化动态命令列表
        // Initialize dynamic command lists
        this->dynamic_cmdlists[i] = this->dynamic_cmdbufs[i].finishList();
        
        // 初始化同步对象
        // Initialize synchronization objects
        this->command_fences[i] = {};
    }

    this->renderer.emplace(1280, 720, this->device, this->queue, *this->pool_images, *this->pool_code, *this->pool_data);
    this->vg = nvgCreateDk(&*this->renderer, NVG_ANTIALIAS | NVG_STENCIL_STROKES);

    // 注册字体到NVG上下文
    // Register fonts to NVG context
    auto standard_font = nvgCreateFontMem(this->vg, "Standard", (unsigned char*)font_standard.address, font_standard.size, 0);
    auto extended_font = nvgCreateFontMem(this->vg, "Extended", (unsigned char*)font_extended.address, font_extended.size, 0);
    nvgAddFallbackFontId(this->vg, standard_font, extended_font);
    

    constexpr PlSharedFontType lang_font[] = {
        PlSharedFontType_ChineseSimplified,
        PlSharedFontType_ExtChineseSimplified,
        PlSharedFontType_ChineseTraditional,
        PlSharedFontType_KO,
    };


    for (auto type : lang_font) {
        if (R_SUCCEEDED(plGetSharedFontByType(&font_lang, type))) {
            char name[32];
            snprintf(name, sizeof(name), "Lang_%u", font_lang.type);
            auto lang_font = nvgCreateFontMem(this->vg, name, (unsigned char*)font_lang.address, font_lang.size, 0);
            nvgAddFallbackFontId(this->vg, standard_font, lang_font);
        } else {
            LOG("failed to load lang font %d\n", type);
        }
    }
    
    
    
    
    // 加载基础图像资源 (Load basic image resources)
    this->default_icon_image = nvgCreateImage(this->vg, "romfs:/default_icon.jpg", NVG_IMAGE_NEAREST);
    this->right_info2 = nvgCreateImage(this->vg, "romfs:/right_info2.jpg", NVG_IMAGE_NEAREST);
    
    // MOD图标将在首次进入MODLIST时延迟加载 (MOD icons will be lazy loaded when first entering MODLIST)
    // 这样可以显著提升应用启动速度 (This significantly improves app startup speed)


    // 启动快速信息扫描
    // Start fast info scanning
    this->async_thread = util::async([this](std::stop_token stop_token){
            this->FastScanNames(stop_token);
            // 名称扫描完成后，初始视口加载由每帧调用自动处理
            // After name scanning is complete, initial viewport loading is handled by per-frame calls
        }
    );

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&this->pad);
    
    // 加载游戏名称映射 (Load game name mapping)
    LoadGameNameMapping();
    
    // 异步初始化音效管理器，避免阻塞启动流程 (Initialize audio manager asynchronously to avoid blocking startup)
    this->audio_manager.InitializeAsync();
}

// 加载模组名称映射文件 (Load mod name mapping file)
bool App::LoadModNameMapping(const std::string& game_name, const std::string& FILE_PATH) {
    // 清除之前的缓存 (Clear previous cache)
    mod_name_cache.clear();
    
    // 设置当前游戏名称 (Set current game name)
    current_game_name = game_name;
    
    // 构建JSON文件路径，包含游戏ID (Build JSON file path, including game ID)
    
    std::string json_path = FILE_PATH + "/mod_name.json";
    
    // 尝试打开文件 (Try to open file)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) {
        return false; // 文件不存在，使用原始显示方式 (File doesn't exist, use original display)
    }
    
    // 获取文件大小 (Get file size)
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return false;
    }
    
    // 读取文件内容 (Read file content)
    std::vector<char> buffer(file_size + 1);
    size_t read_size = fread(buffer.data(), 1, file_size, file);
    fclose(file);
    
    if (read_size != static_cast<size_t>(file_size)) {
        return false;
    }
    
    buffer[file_size] = '\0'; // 确保字符串结尾 (Ensure string termination)
    
    // 解析JSON (Parse JSON)
    yyjson_doc* doc = yyjson_read(buffer.data(), file_size, 0);
    if (!doc) {
        return false; // JSON解析失败 (JSON parsing failed)
    }
    
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }
    
    // 直接遍历根对象中的模组映射 (Directly iterate through mod mappings in root object)
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(root, &iter);
    yyjson_val* key, *val;
    
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = yyjson_obj_iter_get_val(key);
        if (!yyjson_is_obj(val)) continue;
        
        const char* mod_name = yyjson_get_str(key);
        if (!mod_name) continue;
        
        // 获取显示名称 (Get display name)
        yyjson_val* display_name_val = yyjson_obj_get(val, "display_name");
        const char* display_name = yyjson_get_str(display_name_val);
        
        // 获取描述 (Get description)
        yyjson_val* description_val = yyjson_obj_get(val, "description");
        const char* description = yyjson_get_str(description_val);
        
        // 存储到缓存中 (Store in cache)
        ModNameInfo info;
        info.display_name = display_name ? display_name : mod_name;
        info.description = description ? description : "";
        
        mod_name_cache[mod_name] = info;
    }
    
    yyjson_doc_free(doc);
    return true;
}

// 获取映射后的模组名称 (Get mapped mod name)
std::string App::GetMappedModName(const std::string& original_name, const std::string& mod_type) {
    std::unordered_map<std::string, ModNameInfo>::iterator it;
    if (mod_type == NONE_TYPE_TEXT) it = mod_name_cache.find(original_name);
    else it = mod_name_cache.find(original_name + mod_type);
    if (it != mod_name_cache.end()) {
        return it->second.display_name;
    }
    return original_name; // 未找到映射时返回原始名称 (Return original name if mapping not found)
}

// 获取模组描述 (Get mod description)
std::string App::GetModDescription(const std::string& original_name, const std::string& mod_type) {
    std::unordered_map<std::string, ModNameInfo>::iterator it;
    if (mod_type == NONE_TYPE_TEXT) it = mod_name_cache.find(original_name);
    else it = mod_name_cache.find(original_name + mod_type);
    if (it != mod_name_cache.end()) {
        return it->second.description;
    }
    return ""; // 未找到映射时返回空描述 (Return empty description if mapping not found)
}

// 清除模组名称缓存 (Clear mod name cache)
void App::ClearModNameCache() {
    mod_name_cache.clear();
    // 注意：不清除 current_game_name，以保持游戏键缓存 (Note: Don't clear current_game_name to maintain game key cache)
}

// 加载游戏名称映射 (Load game name mapping)
bool App::LoadGameNameMapping() {
    // 构建game_name.json文件路径 (Build game_name.json file path)
    std::string json_path = "/mods2/game_name.json";
    
    
    // 打开文件 (Open file)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) {
        return false; // 文件不存在，使用原始显示方式 (File doesn't exist, use original display)
    }
    
    // 获取文件大小 (Get file size)
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return false;
    }
    
    
    // 读取文件内容 (Read file content)
    std::vector<char> buffer(file_size + 1);
    size_t read_size = fread(buffer.data(), 1, file_size, file);
    fclose(file);
    
    if (read_size != static_cast<size_t>(file_size)) {
        return false;
    }
    
    buffer[file_size] = '\0'; // 确保字符串结尾 (Ensure string termination)
    
    // 解析JSON (Parse JSON)
    yyjson_doc* doc = yyjson_read(buffer.data(), file_size, 0);
    if (!doc) {
        return false; // JSON解析失败 (JSON parsing failed)
    }
    
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }
    
    // 清空现有缓存 (Clear existing cache)
    game_name_cache.clear();
    
    // 遍历根对象中的游戏名称映射 (Iterate through game name mappings in root object)
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(root, &iter);
    yyjson_val* key, *val;
    
    int mapping_count = 0;
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = yyjson_obj_iter_get_val(key);
        
        const char* game_name = yyjson_get_str(key);
        if (!game_name) continue;
        
        // 直接获取值作为显示名称 (Get value directly as display name)
        const char* display_name = yyjson_get_str(val);
        if (!display_name) {
            continue;
        }
        
        // 存储到缓存中 (Store in cache)
        ModNameInfo GAME_NAME;
        GAME_NAME.display_name = display_name;
        
        game_name_cache[game_name] = GAME_NAME;
        mapping_count++;
        
    }
    
    
    yyjson_doc_free(doc);
    return true;
}

// 获取映射后的游戏名称 (Get mapped game name)
std::string App::GetMappedGameName(const std::string& original_name, const std::string& mod_version) {
    std::unordered_map<std::string, ModNameInfo>::iterator it;
    if (mod_version == NONE_TYPE_TEXT) it = game_name_cache.find(original_name);
    else it = game_name_cache.find(original_name + "[" + mod_version + "]");
    if (it != game_name_cache.end()) {
        return it->second.display_name;
    }
    return original_name; // 未找到映射时返回原始名称 (Return original name if mapping not found)
}


// 清空游戏名称映射缓存 (Clear game name mapping cache)
void App::ClearGameNameCache() {
    game_name_cache.clear();
}





/**
 * @brief App类的析构函数
 * 负责清理应用程序使用的所有资源，确保没有内存泄漏
 */
App::~App() {
    // 清理模组名称缓存 (Cleanup mod name cache)
    ClearModNameCache();
    
    // 清理游戏名称缓存 (Cleanup game name cache)
    ClearGameNameCache();
    
    // 请求停止音频异步初始化 (Request to stop audio async initialization)
    this->audio_manager.RequestStop();
    
    // 清理音效管理器 (Cleanup audio manager)
    this->audio_manager.Cleanup();
    
    // 检查异步线程是否有效，如果有效则停止并等待其完成
    if (this->async_thread.valid()) {
        this->async_thread.request_stop(); // 请求线程停止
        this->async_thread.get(); // 等待线程完成
    }
    


    // 遍历所有应用条目，释放自行管理的图像资源
    for (auto&p : this->entries) {
        if (p.own_image) { // 仅释放由应用自己创建的图像
            nvgDeleteImage(this->vg, p.image); // 删除nanovg图像
        }
    }

    // 释放默认图标图像资源 (Release default icon image resources)
    nvgDeleteImage(this->vg, default_icon_image);
    nvgDeleteImage(this->vg, right_info2);
    
    // 只有在MOD图标已加载时才释放 (Only release MOD icons if they were loaded)
    if (mod_icons_loaded) {
        nvgDeleteImage(this->vg, Cheat_code_MOD_image);
        nvgDeleteImage(this->vg, Cosmetic_MOD_image);
        nvgDeleteImage(this->vg, FPS_MOD_image);
        nvgDeleteImage(this->vg, HD_MOD_image);
        nvgDeleteImage(this->vg, More_PLAY_MOD_image);
        nvgDeleteImage(this->vg, NONE_MOD_image);
    }

    // GPU命令优化：等待所有命令缓冲区完成执行
    // GPU command optimization: Wait for all command buffers to complete execution
    for (unsigned i = 0; i < NumCommandBuffers; ++i) {
        this->waitForCommandCompletion(i);
    }
    
    // 销毁帧缓冲区相关资源
    this->destroyFramebufferResources();

    // 删除nanovg上下文
    nvgDeleteDk(this->vg);

    // 刷新缓存文件
    nxtcFlushCacheFile();

    // 退出libnxtc库
    nxtcExit();

    // 重置渲染器指针，释放相关资源
    this->renderer.reset();

    
}

void App::createFramebufferResources() {
    // Create layout for the depth buffer
    dk::ImageLayout layout_depthbuffer;
    dk::ImageLayoutMaker{device}
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_HwCompression)
        .setFormat(DkImageFormat_S8)
        .setDimensions(1280, 720)
        .initialize(layout_depthbuffer);

    // Create the depth buffer
    this->depthBuffer_mem = this->pool_images->allocate(layout_depthbuffer.getSize(), layout_depthbuffer.getAlignment());
    this->depthBuffer.initialize(layout_depthbuffer, this->depthBuffer_mem.getMemBlock(), this->depthBuffer_mem.getOffset());

    // Create layout for the framebuffers
    dk::ImageLayout layout_framebuffer;
    dk::ImageLayoutMaker{device}
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression)
        .setFormat(DkImageFormat_RGBA8_Unorm)
        .setDimensions(1280, 720)
        .initialize(layout_framebuffer);

    // Create the framebuffers
    std::array<DkImage const*, NumFramebuffers> fb_array;
    const uint64_t fb_size  = layout_framebuffer.getSize();
    const uint32_t fb_align = layout_framebuffer.getAlignment();
    for (unsigned i = 0; i < fb_array.size(); i++) {
        // Allocate a framebuffer
        this->framebuffers_mem[i] = pool_images->allocate(fb_size, fb_align);
        this->framebuffers[i].initialize(layout_framebuffer, framebuffers_mem[i].getMemBlock(), framebuffers_mem[i].getOffset());

        // Generate a command list that binds it
        dk::ImageView colorTarget{ framebuffers[i] }, depthTarget{ depthBuffer };
        this->cmdbuf.bindRenderTargets(&colorTarget, &depthTarget);
        this->framebuffer_cmdlists[i] = cmdbuf.finishList();

        // Fill in the array for use later by the swapchain creation code
        fb_array[i] = &framebuffers[i];
    }

    // Create the swapchain using the framebuffers
    // 启用垂直同步以避免画面撕裂
    NWindow* nwin = nwindowGetDefault();
    nwindowSetSwapInterval(nwin, 1); // 设置交换间隔为1，启用垂直同步
    this->swapchain = dk::SwapchainMaker{device, nwin, fb_array}.create();

    // Generate the main rendering cmdlist
    this->recordStaticCommands();
}

void App::destroyFramebufferResources() {
    // Return early if we have nothing to destroy
    if (!this->swapchain) {
        return;
    }

    this->queue.waitIdle();
    this->cmdbuf.clear();
    swapchain.destroy();

    // Destroy the framebuffers
    for (unsigned i = 0; i < NumFramebuffers; i++) {
        framebuffers_mem[i].destroy();
    }

    // Destroy the depth buffer
    this->depthBuffer_mem.destroy();
}

void App::recordStaticCommands() {
    // Initialize state structs with deko3d defaults
    dk::RasterizerState rasterizerState;
    dk::ColorState colorState;
    dk::ColorWriteState colorWriteState;
    dk::BlendState blendState;

    // Configure the viewport and scissor
    this->cmdbuf.setViewports(0, { { 0.0f, 0.0f, 1280, 720, 0.0f, 1.0f } });
    this->cmdbuf.setScissors(0, { { 0, 0, 1280, 720 } });

    // Clear the color and depth buffers
    this->cmdbuf.clearColor(0, DkColorMask_RGBA, 0.2f, 0.3f, 0.3f, 1.0f);
    this->cmdbuf.clearDepthStencil(true, 1.0f, 0xFF, 0);

    // Bind required state
    this->cmdbuf.bindRasterizerState(rasterizerState);
    this->cmdbuf.bindColorState(colorState);
    this->cmdbuf.bindColorWriteState(colorWriteState);

    this->render_cmdlist = this->cmdbuf.finishList();
}

// GPU命令优化：准备下一个命令缓冲区
// GPU command optimization: Prepare next command buffer
void App::prepareNextCommandBuffer() {
    // 等待当前命令缓冲区完成（如果已提交）
    // Wait for current command buffer completion (if submitted)
    if (this->command_submitted[this->current_cmdbuf_index]) {
        this->waitForCommandCompletion(this->current_cmdbuf_index);
        this->command_submitted[this->current_cmdbuf_index] = false;
    }
    
    // 切换到下一个命令缓冲区
    // Switch to next command buffer
    this->current_cmdbuf_index = (this->current_cmdbuf_index + 1) % NumCommandBuffers;
    
    // 重新初始化当前命令缓冲区的命令列表
    // Re-initialize command list for current command buffer
    this->dynamic_cmdlists[this->current_cmdbuf_index] = this->dynamic_cmdbufs[this->current_cmdbuf_index].finishList();
}

// GPU命令优化：提交当前命令缓冲区
// GPU command optimization: Submit current command buffer
void App::submitCurrentCommandBuffer() {
    // 设置同步点
    // Set synchronization point
    this->dynamic_cmdbufs[this->current_cmdbuf_index].signalFence(this->command_fences[this->current_cmdbuf_index]);
    
    // 提交命令列表
    // Submit command list
    this->queue.submitCommands(this->dynamic_cmdlists[this->current_cmdbuf_index]);
    this->command_submitted[this->current_cmdbuf_index] = true;
}

// GPU命令优化：等待指定命令缓冲区完成
// GPU command optimization: Wait for specified command buffer completion
void App::waitForCommandCompletion(unsigned buffer_index) {
    if (buffer_index < NumCommandBuffers && this->command_submitted[buffer_index]) {
        // 等待fence信号
        // Wait for fence signal
        this->command_fences[buffer_index].wait();
    }
}

// 对话框相关函数实现 (Dialog related function implementations)

// 显示对话框 (Show dialog)
void App::ShowDialog(DialogType type, const std::string& content,
                     float content_font_size, int content_align) {
    this->dialog_type = type;
    this->dialog_content = content;
    this->dialog_content_font_size = content_font_size;
    this->dialog_content_align = content_align;
    this->show_dialog = true;
    this->dialog_selected_ok = true; // 默认选中确定按钮 (Default to OK button)
}



// 显示复制进度对话框 (Show copy progress dialog)
void App::ShowCopyProgressDialog(const std::string& title) {
    this->dialog_type = DialogType::COPY_PROGRESS;
    
    this->dialog_content = title;

    this->show_dialog = true;
}



// 更新复制进度 (Update copy progress)
void App::UpdateCopyProgress(const CopyProgressInfo& progress) {
    std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
    this->copy_progress = progress;
}

// 隐藏对话框 (Hide dialog)
void App::HideDialog() {
    this->show_dialog = false;
    this->dialog_type = DialogType::INFO;
    this->dialog_content.clear();
    
}

// 绘制对话框 (Draw dialog)
void App::DrawDialog() {
    if (!this->show_dialog) return;
    
    const float dialog_width = 650.0f;
    const float dialog_height = 350.0f;
    // 对话框位置 (Dialog position)
    const float dialog_x = (SCREEN_WIDTH - dialog_width) / 2.0f;
    const float dialog_y = (SCREEN_HEIGHT - dialog_height) / 2.0f;
    
    // 绘制半透明背景遮罩 (Draw semi-transparent background overlay)
    nvgBeginPath(this->vg);
    nvgRect(this->vg, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    nvgFillColor(this->vg, nvgRGBA(0, 0, 0, 128));
    nvgFill(this->vg);
    
    // 绘制对话框背景 (Draw dialog background)
    nvgBeginPath(this->vg);
    nvgRoundedRect(this->vg, dialog_x, dialog_y, dialog_width, dialog_height, 6.0f);
    nvgFillColor(this->vg, nvgRGB(45, 45, 45));
    nvgFill(this->vg);
    

    
    // 复制进度对话框的绘制 (Draw copy progress dialog)
    if (this->dialog_type == DialogType::COPY_PROGRESS) {

        // 获取复制进度信息 (Get copy progress info)
        CopyProgressInfo progress_info;
        {
            std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
            progress_info = this->copy_progress;
        }
        
        // 绘制标题栏分界线 (Draw title divider line)
        nvgStrokeColor(this->vg, nvgRGB(100, 100, 100));
        nvgStrokeWidth(this->vg, 1.0f);
        nvgBeginPath(this->vg);
        nvgMoveTo(this->vg, dialog_x, dialog_y + 70.0f);
        nvgLineTo(this->vg, dialog_x + dialog_width, dialog_y + 70.0f);
        nvgStroke(this->vg);
        
        // 绘制标题 (Draw title)
        gfx::drawTextBoxCentered(this->vg, dialog_x, dialog_y, dialog_width, 70.0f,
                         24.0f, 1.4f,
                         this->dialog_content.c_str(), nullptr,
                         NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE);
        
        
        // 进度条位置计算 (Calculate progress bar position)
        const float progress_bar_width = dialog_width - 120;
        const float progress_bar_height = 30.0f;
        const float progress_bar_x = dialog_x + (dialog_width - progress_bar_width) / 2;
        const float progress_bar_y = dialog_y + dialog_height * 3 / 4 - progress_bar_height / 2;
        
        // 绘制进度条背景 (Draw progress bar background)
        nvgBeginPath(this->vg);
        nvgRoundedRect(this->vg, progress_bar_x, progress_bar_y, progress_bar_width, progress_bar_height, 6.0f);
        nvgFillColor(this->vg, nvgRGB(60, 60, 60));
        nvgFill(this->vg);
        
        // 绘制进度条填充 (Draw progress bar fill)
        if (progress_info.progress_percentage > 0.0f) {
            const float fill_width = progress_bar_width * (progress_info.progress_percentage / 100.0f);
            nvgBeginPath(this->vg);
            nvgRoundedRect(this->vg, progress_bar_x, progress_bar_y, fill_width, progress_bar_height, 6.0f);
            nvgFillColor(this->vg, nvgRGB(0, 150, 255)); // 蓝色进度条 (Blue progress bar)
            nvgFill(this->vg);
        }

        gfx::drawTextBoxCentered(this->vg, dialog_x, dialog_y + dialog_height / 4 , dialog_width, dialog_height / 4 + 40.f, 
                             24.0f , 1.4f, 
                             this->mod_info[this->mod_index].MOD_NAME2.c_str(), nullptr, 
                             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE);





        // 绘制当前复制的文件名 (Draw current copying file name)
        if (!progress_info.current_file.empty()) {
            gfx::drawTextBoxCentered(this->vg, progress_bar_x +3.f, progress_bar_y-35.0f, progress_bar_width - 6.f, 35.0f ,
                             20.0f, 1.4f,
                             progress_info.current_file.c_str(), nullptr,
                             NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE);
        }

        // 文件数量背景 ，防止被文件名挡住了
        nvgBeginPath(this->vg);
        nvgRoundedRect(this->vg, dialog_x + dialog_width * 0.65, progress_bar_y-45.0f, dialog_width * 0.35, 45.0f , 0.0f);
        nvgFillColor(this->vg, nvgRGB(45, 45, 45));
        nvgFill(this->vg);

        // 绘制文件进度信息 (Draw file progress info)
        char progress_text[128];
        snprintf(progress_text, sizeof(progress_text), "%zu / %zu",
                progress_info.files_copied, progress_info.total_files);
        gfx::drawTextBoxCentered(this->vg, progress_bar_x + 3.f, progress_bar_y-35.0f, progress_bar_width - 6.f, 35.0f ,
                         20.0f, 1.4f,
                         progress_text, nullptr,
                         NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE);

        
        
        // 绘制进度百分比文字 (Draw progress percentage text)
        nvgFontSize(this->vg, 18.0f);
        nvgTextAlign(this->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(this->vg, nvgRGB(255, 255, 255));
        snprintf(progress_text, sizeof(progress_text), "%.1f%%", progress_info.progress_percentage);
        nvgText(this->vg, progress_bar_x + progress_bar_width / 2.0f, progress_bar_y + progress_bar_height / 2.0f, progress_text, nullptr);
        
        // 绘制文件级进度条 (Draw file-level progress bar)
        if (progress_info.is_copying_file && progress_info.file_progress_percentage > 0.0f) {
            const float file_progress_bar_y = progress_bar_y + progress_bar_height + 10.0f; // 位于总进度条下方10像素 (10 pixels below total progress bar)
            const float file_progress_bar_height = 3.0f; // 宽度3像素 (3 pixels width)
            const float file_fill_width = progress_bar_width * (progress_info.file_progress_percentage / 100.0f);
            
            // 绘制文件级进度条填充 (Draw file-level progress bar fill)
            nvgBeginPath(this->vg);
            nvgRect(this->vg, progress_bar_x, file_progress_bar_y, file_fill_width, file_progress_bar_height);
            nvgFillColor(this->vg, nvgRGB(0, 150, 255)); // 绿色进度条 (Green progress bar)
            nvgFill(this->vg);
        }
        
        return; // 复制进度对话框不绘制按钮 (Copy progress dialog doesn't draw buttons)
    }

    // 按钮尺寸和位置 (Button size and position)
    const float button_height = 70.0f;
    // 计算布局参数 (Calculate layout parameters)
    const float content_height = dialog_height - button_height; // 内容区域高度 (Content area height)
    const float button_width = dialog_width / 2.0f; // 每个按钮占对话框一半宽度 (Each button takes half dialog width)
    const float buttons_y = dialog_y + dialog_height - button_height; // 按钮Y坐标 (Button Y coordinate)

    // 绘制内容 (Draw content) - 使用drawTextBoxCentered在除按钮外的全部区域居中显示文本
    gfx::drawTextBoxCentered(this->vg, dialog_x + 80.f, dialog_y + 40.f, dialog_width - 160.f, content_height - 80.f, 
                             this->dialog_content_font_size, 1.4f, 
                             this->dialog_content.c_str(), nullptr, 
                             this->dialog_content_align, gfx::Colour::WHITE);
    
    // 先绘制按钮分界线 (Draw button divider lines first)
    nvgStrokeColor(this->vg, nvgRGB(100, 100, 100)); // 与边框相同的颜色 (Same color as border)
    nvgStrokeWidth(this->vg, 2.0f);
    
    // 绘制按钮顶部分界线 (Draw top divider line)
    nvgBeginPath(this->vg);
    nvgMoveTo(this->vg, dialog_x, buttons_y);
    nvgLineTo(this->vg, dialog_x + dialog_width, buttons_y);
    nvgStroke(this->vg);
    
    // 绘制按钮中间分界线 (Draw middle divider line)
    nvgBeginPath(this->vg);
    nvgMoveTo(this->vg, dialog_x + button_width, buttons_y);
    nvgLineTo(this->vg, dialog_x + button_width, buttons_y + button_height);
    nvgStroke(this->vg);
    
    // 先绘制所有按钮的基础背景 (First draw all button backgrounds)
    // 确定按钮背景 (OK button background)
    nvgBeginPath(this->vg);
    nvgRoundedRectVarying(this->vg, dialog_x, buttons_y, button_width, button_height, 
                          0.0f, 0.0f, 0.0f, 6.0f); // 左半部分，左下角圆角 (Left half, bottom-left corner rounded)
    nvgFillColor(this->vg, nvgRGB(45, 45, 45));
    nvgFill(this->vg);
    
    // 取消按钮背景 (Cancel button background)
    nvgBeginPath(this->vg);
    nvgRoundedRectVarying(this->vg, dialog_x + button_width, buttons_y, button_width, button_height, 
                          0.0f, 0.0f, 6.0f, 0.0f); // 右半部分，右下角圆角 (Right half, bottom-right corner rounded)
    nvgFillColor(this->vg, nvgRGB(45, 45, 45));
    nvgFill(this->vg);
    
    // 然后绘制选中状态的彩色边框 (Then draw selected state colored borders)
    auto col = pulse.col;  // 获取脉冲颜色 (Get pulse color)
    col.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
    col.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
    col.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
    col.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
    // 更新脉冲颜色动画 (Update pulse color animation)
    update_pulse_colour();
    
    if (this->dialog_selected_ok) {
        // 确定按钮选中：绘制彩色边框 (OK button selected: draw colored border)
        nvgBeginPath(this->vg);
        nvgRoundedRect(this->vg, dialog_x - 3.f, buttons_y - 3.f, button_width + 6.f, button_height + 6.f, 6.0f);
        nvgFillColor(this->vg, nvgRGBAf(col.r, col.g, col.b, col.a));
        nvgFill(this->vg);
        // 重新绘制确定按钮的黑色背景 (Redraw OK button black background)
        nvgBeginPath(this->vg);
        nvgRoundedRectVarying(this->vg, dialog_x, buttons_y, button_width, button_height, 
                              0.0f, 0.0f, 0.0f, 6.0f);
        nvgFillColor(this->vg, nvgRGB(45, 45, 45));
        nvgFill(this->vg);
    } else {
        // 取消按钮选中：绘制彩色边框 (Cancel button selected: draw colored border)
        nvgBeginPath(this->vg);
        nvgRoundedRect(this->vg, dialog_x + button_width - 3.f, buttons_y - 3.f, button_width + 6.f, button_height + 6.f, 6.0f);
        nvgFillColor(this->vg, nvgRGBAf(col.r, col.g, col.b, col.a));
        nvgFill(this->vg);
        // 重新绘制取消按钮的黑色背景 (Redraw Cancel button black background)
        nvgBeginPath(this->vg);
        nvgRoundedRectVarying(this->vg, dialog_x + button_width, buttons_y, button_width, button_height, 
                              0.0f, 0.0f, 6.0f, 0.0f);
        nvgFillColor(this->vg, nvgRGB(45, 45, 45));
        nvgFill(this->vg);
    }
    
    // 绘制按钮文字 (Draw button text)
    nvgFontSize(this->vg, 24.0f);
    nvgTextAlign(this->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(this->vg, nvgRGB(255, 255, 255));
    
    // 确定按钮文字 (OK button text)
    nvgText(this->vg, dialog_x + button_width / 2.0f, buttons_y + button_height / 2.0f, 
            this->dialog_button_ok.c_str(), nullptr);
    
    // 取消按钮文字 (Cancel button text)
    nvgText(this->vg, dialog_x + button_width + button_width / 2.0f, 
            buttons_y + button_height / 2.0f, this->dialog_button_cancel.c_str(), nullptr);

}

// 更新对话框输入 (Update dialog input)
void App::UpdateDialog() {
    if (!this->show_dialog) return;

    

    // 处理复制进度对话框 (Handle copy progress dialog)
    if (this->dialog_type == DialogType::COPY_PROGRESS) {
        
        std::string MOD_NAME = this->mod_info[this->mod_index].MOD_NAME2;
        bool MOD_STATE = this->mod_info[this->mod_index].MOD_STATE;
        
        // 检查复制任务是否完成 (Check if copy task is completed)
        bool task_completed = false;
        bool task_result = false;
        
        if (copy_task.valid() && copy_task.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            task_result = copy_task.get();
            task_completed = true;
        }
        
        
        if (task_completed) {
            // 任务完成后隐藏对话框 (Hide dialog after task completion)
            this->HideDialog();
            
            // 显示结果对话框 (Show result dialog)
            if (task_result) {
                
                this->audio_manager.PlayConfirmSound();

                std::string duration_str = FormatDuration(this->operation_duration.count() / 1000);
                
                if (MOD_STATE || clean_button) this->ShowDialog(DialogType::INFO, GetSnprintf(SUCCESS_UNINSTALLED, MOD_NAME, duration_str), 26.0f, NVG_ALIGN_CENTER);
                        else this->ShowDialog(DialogType::INFO, GetSnprintf(SUCCESS_INSTALLED, MOD_NAME, duration_str), 26.0f, NVG_ALIGN_CENTER);
                
                if (MOD_STATE && clean_button) {
                    this->ChangeModName();
                } else if (!clean_button) {
                    this->ChangeModName();
                }
                
                this->clean_button = false;

            } else {

                std::string error_msg;
                if (MOD_STATE || clean_button) {
                    error_msg = GetSnprintf(FAILURE_UNINSTALLED,MOD_NAME);
                    this->clean_button = false;
                }
                else error_msg = GetSnprintf(FAILURE_INSTALLED,MOD_NAME);
                
                {
                    std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                    // 如果有具体的错误信息，优先显示具体错误信息 (If there's specific error message, prioritize showing it)
                    if (!this->copy_progress.error_message.empty()) {
                        error_msg = this->copy_progress.error_message;
                    } 
                }

                this->audio_manager.PlayCancelSound();
                this->ShowDialog(DialogType::INFO, error_msg, 26.0f, NVG_ALIGN_CENTER);
            }
            return;
        }
        
        // 允许用户按B键取消复制 (Allow user to press B to cancel copy)
        if (this->controller.B) {
            this->audio_manager.PlayConfirmSound();
            if (copy_task.valid()) {
                copy_task.request_stop(); // 请求停止复制任务 (Request to stop copy task)
            }
            this->HideDialog();
            this->audio_manager.PlayCancelSound();
            if (MOD_STATE && !clean_button) {
                this->ShowDialog(DialogType::INFO, GetSnprintf(CANCEL_UNINSTALLED,MOD_NAME), 26.0f, NVG_ALIGN_CENTER);
                this->clean_button = false;
            } else this->ShowDialog(DialogType::INFO, GetSnprintf(CANCEL_INSTALLED,MOD_NAME), 26.0f, NVG_ALIGN_CENTER);
            return;
        }
    }


    if(this->dialog_type == DialogType::CONFIRM) {
        // 左右键切换按钮选择 (Left/Right keys to switch button selection)
        if (this->controller.LEFT && !this->dialog_selected_ok) {
            // 在取消按钮时按左键，切换到确定按钮 (Press left on cancel button to switch to OK button)
            this->audio_manager.PlayKeySound(0.9);
            this->dialog_selected_ok = true;
        } else if (this->controller.LEFT) {
            this->audio_manager.PlayLimitSound(1.5);
        }else if (this->controller.RIGHT && this->dialog_selected_ok) {
            // 在确定按钮时按右键，切换到取消按钮 (Press right on OK button to switch to cancel button)
            this->audio_manager.PlayKeySound(0.9);
            this->dialog_selected_ok = false;
        } else if (this->controller.RIGHT) {
            this->audio_manager.PlayLimitSound(1.5);
        } else if (this->controller.A && this->dialog_selected_ok) {

            this->audio_manager.PlayConfirmSound(); // 播放确认音效 (Play confirm sound effect)

            bool select_mod_state = this->mod_info[this->mod_index].MOD_STATE;
            int result = 0;
            
            if (select_mod_state || clean_button) {
                result = ModUninstalled(); // 卸载仍使用原方法 (Uninstall still uses original method)
            } else {
                result = ModInstalled();
            }
            
            // 如果返回0，表示没有有效文件夹，不隐藏对话框 (If returns 0, means no valid folders, don't hide dialog)
            if (result == 0) {
                return; // 保持对话框显示 (Keep dialog visible)
            }

            
        }else if (this->controller.B || (this->controller.A && !this->dialog_selected_ok)) {
            this->clean_button = false;
            this->audio_manager.PlayConfirmSound(); // 播放确认音效 (Play confirm sound effect)
            this->HideDialog(); // 隐藏对话框 (Hide dialog)
        }
    }


    if(this->dialog_type == DialogType::INFO) {
        if (this->controller.LEFT && !this->dialog_selected_ok) {
            // 在取消按钮时按左键，切换到确定按钮 (Press left on cancel button to switch to OK button)
            this->audio_manager.PlayKeySound(0.9);
            this->dialog_selected_ok = true;
        } else if (this->controller.LEFT) {
            this->audio_manager.PlayLimitSound(1.5);
        }else if (this->controller.RIGHT && this->dialog_selected_ok) {
            // 在确定按钮时按右键，切换到取消按钮 (Press right on OK button to switch to cancel button)
            this->audio_manager.PlayKeySound(0.9);
            this->dialog_selected_ok = false;
        } else if (this->controller.RIGHT) {
            this->audio_manager.PlayLimitSound(1.5);
        } else if (this->controller.A) {
            this->audio_manager.PlayConfirmSound(); // 播放确认音效 (Play confirm sound effect)
            this->HideDialog(); // 隐藏对话框 (Hide dialog)
        } else if (this->controller.B) {
            this->audio_manager.PlayConfirmSound(); // 播放确认音效 (Play confirm sound effect)
            this->HideDialog(); // 隐藏对话框 (Hide dialog)
        }

    }



}

// 更新教程界面 (Update instruction interface)
void App::UpdateInstruction() {
    // B键返回到主菜单 (B key to return to main menu)
    if (this->controller.B) {
        this->audio_manager.PlayKeySound(); 
        this->menu_mode = MenuMode::LIST; // 返回到应用列表界面 (Return to application list interface)
    }
    
    // LEFT键：上一页 (LEFT key: previous page)
    if ((this->controller.L || this->controller.LEFT) && this->instruction_current_page > 0) {
        this->audio_manager.PlayKeySound();
        this->instruction_current_page--;
    }
    
    // RIGHT键：下一页 (RIGHT key: next page)
    constexpr int TOTAL_LINES = 24;
    constexpr int LINES_PER_PAGE = 12;
    const int total_pages = (TOTAL_LINES + LINES_PER_PAGE - 1) / LINES_PER_PAGE;

    if ((this->controller.R || this->controller.RIGHT) && this->instruction_current_page < total_pages - 1) {
        this->audio_manager.PlayKeySound();
        this->instruction_current_page++;
    }
}

// 绘制教程界面 (Draw instruction interface)
void App::DrawInstruction() {
    // 简化的教程界面实现 (Simplified instruction interface implementation)
    constexpr int LINES_PER_PAGE = 12;  // 每页显示行数 (Lines per page)
    constexpr int TOTAL_LINES = 24;     // 总行数 (Total lines)
    const int total_pages = (TOTAL_LINES + LINES_PER_PAGE - 1) / LINES_PER_PAGE; // 总页数 (Total pages)
    
    // 绘制底部按钮提示 (Draw bottom button hints)
    // 根据页码状态设置按钮颜色 (Set button colors based on page status)
    gfx::Colour left_color = (this->instruction_current_page > 0) ? gfx::Colour::WHITE : gfx::Colour::GREY;
    gfx::Colour right_color = (this->instruction_current_page < total_pages - 1) ? gfx::Colour::WHITE : gfx::Colour::GREY;
    
    // 使用支持单独颜色设置的按钮绘制函数 (Use button drawing function with individual color support)
    gfx::drawButtons2Colored(this->vg,
        gfx::make_pair2_colored(gfx::Button::B, BUTTON_BACK.c_str(), gfx::Colour::WHITE),
        gfx::make_pair2_colored(gfx::Button::R, "下翻", right_color),
        gfx::make_pair2_colored(gfx::Button::L, "上翻", left_color)
    );


    // 绘制标题 (Draw title)
    SOFTWARE_TITLE = SOFTWARE_TITLE_INSTRUCTION;
    
    // 教程文本内容数组 (Instruction text content array)
    const std::string* instructions[] = {
        &INSTRUCTION_LINE_1, &INSTRUCTION_LINE_2, &INSTRUCTION_LINE_3, &INSTRUCTION_LINE_4,
        &INSTRUCTION_LINE_5, &INSTRUCTION_LINE_6, &INSTRUCTION_LINE_7, &INSTRUCTION_LINE_8,
        &INSTRUCTION_LINE_9, &INSTRUCTION_LINE_10, &INSTRUCTION_LINE_11, &INSTRUCTION_LINE_12,
        &INSTRUCTION_LINE_13, &INSTRUCTION_LINE_14, &INSTRUCTION_LINE_15, &INSTRUCTION_LINE_16,
        &INSTRUCTION_LINE_17, &INSTRUCTION_LINE_18, &INSTRUCTION_LINE_19, &INSTRUCTION_LINE_20,
        &INSTRUCTION_LINE_21, &INSTRUCTION_LINE_22, &INSTRUCTION_LINE_23, &INSTRUCTION_LINE_24
    };
    
    // 计算当前页显示范围 (Calculate current page display range)
    const int start_line = this->instruction_current_page * LINES_PER_PAGE;
    const int end_line = std::min(start_line + LINES_PER_PAGE, TOTAL_LINES);
    
    // 绘制当前页的教程文本 (Draw instruction text for current page)
    const float content_y = 147.f;
    const float line_height = 40.f;
    const float content_x = 50.f;
    
    for (int i = start_line; i < end_line; i++) {
        const int display_index = i - start_line;
        gfx::drawTextArgs(this->vg, content_x, content_y + display_index * line_height, 22.f,
                         NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE,
                         gfx::Colour::WHITE, instructions[i]->c_str());
    }
    
    // 绘制页码 (Draw page number)
    const std::string page_info = std::to_string(this->instruction_current_page + 1) + "/" + std::to_string(total_pages);
    gfx::drawTextArgs(this->vg, 50.f, 675.f, 20.f,
                     NVG_ALIGN_LEFT | NVG_ALIGN_TOP,
                     gfx::Colour::WHITE, page_info.c_str());
    
    // 总页数已在局部计算，无需存储 (Total pages calculated locally, no need to store)
    
    nvgRestore(this->vg);
}

// 切换当前选中模组的安装状态并修改目录名称 (Toggle current selected mod install status and modify directory name)
void App::ChangeModName() {
    
    // 获取当前选中的模组
    MODINFO& current_mod = mod_info[mod_index];
    
    // 获取当前的目录路径
    std::string current_path = current_mod.MOD_PATH;
    std::string new_path;
    
    // 根据当前状态决定新的路径名
    if (current_mod.MOD_STATE) {
        // 当前已安装，改为未安装，移除末尾的$符号
        if (!current_path.empty() && current_path.back() == '$') {
            new_path = current_path.substr(0, current_path.length() - 1);
        } else {
            return; // 如果没有$符号，说明状态不一致，直接返回
        }
    } else {
        // 当前未安装，改为已安装，在末尾添加$符号
        if (!current_path.empty() && current_path.back() != '$') {
            new_path = current_path + "$";
        } else {
            return; // 如果已有$符号，说明状态不一致，直接返回
        }
    }
    
    // 尝试重命名目录
    if (rename(current_path.c_str(), new_path.c_str()) == 0) {
        // 重命名成功，更新模组信息
        current_mod.MOD_STATE = !current_mod.MOD_STATE;
        current_mod.MOD_PATH = new_path;
    }
    // 如果重命名失败，不做任何更改
}


// 排序MOD列表函数 (Sort MOD list function)
void App::Sort_Mod() {
    std::sort(this->mod_info.begin(), this->mod_info.end(), [](const MODINFO& a, const MODINFO& b) {
        // 首先按安装状态排序：已安装的在前面 (First sort by installation status: installed ones first)
        if (a.MOD_STATE != b.MOD_STATE) {
            return a.MOD_STATE > b.MOD_STATE; // true > false，已安装的排在前面 (true > false, installed ones first)
        }
        
        // 在相同安装状态下，按类型排序 (Within same installation status, sort by type)
        // 定义类型优先级：有类型的在前面，无类型的在后面 (Define type priority: with type first, without type last)
        bool a_has_type = !a.MOD_TYPE.empty();
        bool b_has_type = !b.MOD_TYPE.empty();
        
        if (a_has_type != b_has_type) {
            return a_has_type > b_has_type; // 有类型的排在前面 (With type first)
        }
        
        // 如果都有类型或都没有类型，按字母顺序排序 (If both have type or both don't have type, sort alphabetically)
        if (a_has_type && b_has_type) {
            // 都有类型，先按类型排序，类型相同则按名称排序 (Both have type, sort by type first, then by name if same type)
            if (a.MOD_TYPE != b.MOD_TYPE) {
                return a.MOD_TYPE < b.MOD_TYPE; // 按类型字母顺序 (Sort by type alphabetically)
            }
            return a.MOD_NAME2 < b.MOD_NAME2; // 类型相同，按名称字母顺序 (Same type, sort by name alphabetically)
        } else {
            // 都没有类型，直接按名称排序 (Both don't have type, sort by name directly)
            return a.MOD_NAME2 < b.MOD_NAME2; // 按名称字母顺序 (Sort by name alphabetically)
        }
    });
}


int App::ModInstalled() {
    // 如果有正在运行的任务，检查是否已停止 (If there's a running task, check if it has stopped)
    if (copy_task.valid()) {
        auto status = copy_task.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::timeout) {
            // 任务仍在运行，显示提示对话框 (Task still running, show prompt dialog)
            this->audio_manager.PlayCancelSound();
            this->ShowDialog(DialogType::INFO, DNOT_READY, 26.0f, NVG_ALIGN_CENTER);
            return 0; // 返回0表示操作被阻止 (Return 0 to indicate operation blocked)
        }
        copy_task.request_stop(); // 请求停止当前任务 (Request to stop current task)
    }
    
    
    // 检查是否有选中的MOD (Check if there is a selected MOD)
    if (this->mod_info.empty() || this->mod_index >= this->mod_info.size()) {
        return 0; // 没有选中的MOD (No selected MOD)
    }
    
    // 获取当前选中MOD的路径 (Get current selected MOD path)
    std::string mod_path = this->mod_info[this->mod_index].MOD_PATH;
    
    // 立即显示安装进度对话框 (Immediately show installation progress dialog)
    ShowCopyProgressDialog(INSTALLEDING_TEXT);
    
    // 初始化进度信息 (Initialize progress info)
    {
        std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
        this->copy_progress = {};
    }
    
    // 启动异步安装任务 (Start async installation task)
    copy_task = util::async([this, mod_path](std::stop_token stop_token) -> bool {
        // 开始计时 (Start timing)
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 创建适配的进度回调函数 (Create adapted progress callback function)
        auto mod_progress_callback = [this](int current, int total, const std::string& current_file, bool is_copying_file, float file_progress_percentage) {
            CopyProgressInfo progress;
            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                progress = this->copy_progress;
            }
            
            // 直接使用传入的参数更新所有进度信息 (Update all progress information with passed parameters)
            progress.files_copied = current;
            progress.total_files = total;
            progress.current_file = current_file;
            progress.progress_percentage = total > 0 ? (float)current / total * 100.0f : 0.0f;
            progress.is_copying_file = is_copying_file;
            progress.file_progress_percentage = file_progress_percentage;
            
            this->UpdateCopyProgress(progress);
        };
        
        // 创建错误回调函数 (Create error callback function)
        auto error_callback = [this](const std::string& error_msg) {
            CopyProgressInfo error_progress;
            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                error_progress = this->copy_progress;
            }
            error_progress.has_error = true;
            error_progress.error_message = error_msg;
            this->UpdateCopyProgress(error_progress);
        };
        
        // 使用mod_manager的统一安装接口 (Use mod_manager's unified installation interface)
        bool install_result = this->mod_manager.getModInstallType(
            mod_path,
            1, // operation_type: 1=删除ZIP文件 (1=delete ZIP file)
            mod_progress_callback,
            error_callback,
            stop_token
        );
        
        // 安装完成，计算耗时并更新状态 (Installation completed, calculate duration and update status)
        auto end_time = std::chrono::high_resolution_clock::now();
        this->operation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);


        CopyProgressInfo final_progress;
        {
            std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
            final_progress = this->copy_progress;
        }
        
        final_progress.is_completed = true;
        final_progress.has_error = !install_result;
        // 只有在没有错误信息且安装失败时才设置默认错误信息 (Only set default error message if no error message exists and installation failed)
        if (!install_result && final_progress.error_message.empty()) {
            final_progress.error_message = COPY_ERROR;
        }
        // 如果安装失败但有具体错误信息，保持原有错误信息 (If installation failed but has specific error message, keep original error message)
        this->UpdateCopyProgress(final_progress);


        return install_result;
    });
    
    return 1; // 返回1表示开始了异步安装 (Return 1 to indicate async installation started)
}


// 卸载选中的MOD从atmosphere目录 (Uninstall selected MOD from atmosphere directory)
int App::ModUninstalled() {
    // 如果有正在运行的任务，检查是否已停止 (If there's a running task, check if it has stopped)
    if (copy_task.valid()) {
        auto status = copy_task.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::timeout) {
            // 任务仍在运行，显示提示对话框 (Task still running, show prompt dialog)
            this->audio_manager.PlayCancelSound();
            this->ShowDialog(DialogType::INFO, DNOT_READY, 26.0f, NVG_ALIGN_CENTER);
            return 0; // 返回0表示操作被阻止 (Return 0 to indicate operation blocked)
        }
        copy_task.request_stop(); // 请求停止当前任务 (Request to stop current task)
    }
    
    
    // 检查是否有选中的MOD (Check if there is a selected MOD)
    if (this->mod_info.empty() || this->mod_index >= this->mod_info.size()) {
        return 0; // 没有选中的MOD (No selected MOD)
    }
    
    // 获取当前选中MOD的路径 (Get current selected MOD path)
    std::string mod_path = this->mod_info[this->mod_index].MOD_PATH;
    
    // 立即显示卸载进度对话框 (Immediately show uninstall progress dialog)
    ShowCopyProgressDialog(UNINSTALLEDING_TEXT);
    
    // 初始化进度信息，总文件数将在异步任务中更新 (Initialize progress info, total files will be updated in async task)
    {
        std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
        this->copy_progress = {};
    }
    
    // 启动异步卸载任务 (Start async uninstall task)
    copy_task = util::async([this, mod_path](std::stop_token stop_token) -> bool {
        // 开始计时 (Start timing)
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 创建进度回调函数 (Create progress callback function)
        auto mod_progress_callback = [this](int current, int total, const std::string& filename, bool is_copying_file, float file_progress_percentage) {
            CopyProgressInfo progress;
            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                progress = this->copy_progress;
            }
            progress.files_copied = current;
            progress.total_files = total;
            progress.current_file = filename;
            progress.progress_percentage = total > 0 ? (float)current / total * 100.0f : 0.0f;
            this->UpdateCopyProgress(progress);
        };
        
        // 创建错误回调函数 (Create error callback function)
        auto error_callback = [this](const std::string& error_message) {
            CopyProgressInfo progress;
            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                progress = this->copy_progress;
            }
            progress.error_message = error_message;
            progress.has_error = true;
            this->UpdateCopyProgress(progress);
        };
        
        // 使用mod_manager的统一卸载接口 (Use mod_manager's unified uninstall interface)
        bool uninstall_result = this->mod_manager.getModInstallType(
            mod_path,
            0, // operation_type: 0=卸载 (0=uninstall)
            mod_progress_callback,
            error_callback,
            stop_token
        );
        
        // 卸载完成，计算耗时并更新状态 (Uninstall completed, calculate duration and update status)
        auto end_time = std::chrono::high_resolution_clock::now();
        this->operation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        CopyProgressInfo final_progress;
        {
            std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
            final_progress = this->copy_progress;
        }
        final_progress.is_completed = true;
        final_progress.has_error = !uninstall_result;
        if (!uninstall_result && final_progress.error_message.empty()) {
            final_progress.error_message = UNINSTALLED_ERROR;
        }
        this->UpdateCopyProgress(final_progress);
        
        return uninstall_result;
    });
    
    return 1; // 返回1表示开始了异步卸载 (Return 1 to indicate async uninstall started)
}


} // namespace tj
