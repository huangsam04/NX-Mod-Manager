#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace tj {

// 日志系统类 (Logger system class)
class Logger {
public:
    // 日志级别枚举 (Log level enumeration)
    enum class Level {
        DEBUG = 0,   // 调试信息 (Debug information)
        INFO = 1,    // 一般信息 (General information)
        WARNING = 2, // 警告信息 (Warning information)
        ERROR = 3,   // 错误信息 (Error information)
        CRITICAL = 4 // 严重错误 (Critical error)
    };
    
    // 获取单例实例 (Get singleton instance)
    static Logger& GetInstance();
    
    // 设置日志级别 (Set log level)
    static void SetLevel(Level level);
    
    // 设置日志文件路径 (Set log file path)
    static void SetLogFile(const std::string& file_path);
    
    // 启用/禁用控制台输出 (Enable/disable console output)
    static void SetConsoleOutput(bool enabled);
    
    // 启用/禁用文件输出 (Enable/disable file output)
    static void SetFileOutput(bool enabled);
    
    // 日志记录方法 (Log recording methods)
    static void Debug(const std::string& message);
    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);
    static void Critical(const std::string& message);
    
    // 格式化日志记录方法 (Formatted log recording methods)
    template<typename... Args>
    static void Debug(const std::string& format, Args&&... args) {
        GetInstance().Log(Level::DEBUG, FormatString(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    static void Info(const std::string& format, Args&&... args) {
        GetInstance().Log(Level::INFO, FormatString(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    static void Warning(const std::string& format, Args&&... args) {
        GetInstance().Log(Level::WARNING, FormatString(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    static void Error(const std::string& format, Args&&... args) {
        GetInstance().Log(Level::ERROR, FormatString(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    static void Critical(const std::string& format, Args&&... args) {
        GetInstance().Log(Level::CRITICAL, FormatString(format, std::forward<Args>(args)...));
    }
    
    // 刷新日志缓冲区 (Flush log buffer)
    static void Flush();
    
private:
    Logger() = default;
    ~Logger();
    
    // 禁止拷贝构造和赋值 (Disable copy construction and assignment)
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // 内部日志记录方法 (Internal log recording method)
    void Log(Level level, const std::string& message);
    
    // 获取当前时间戳字符串 (Get current timestamp string)
    std::string GetTimestamp() const;
    
    // 获取日志级别字符串 (Get log level string)
    std::string GetLevelString(Level level) const;
    
    // 字符串格式化辅助方法 (String formatting helper method)
    template<typename... Args>
    static std::string FormatString(const std::string& format, Args&&... args) {
        // 简单的字符串格式化实现 (Simple string formatting implementation)
        // 注意：这是一个简化版本，实际项目中可能需要更复杂的实现
        // Note: This is a simplified version, actual projects may need more complex implementation
        std::ostringstream oss;
        FormatStringImpl(oss, format, std::forward<Args>(args)...);
        return oss.str();
    }
    
    // 递归格式化实现 (Recursive formatting implementation)
    template<typename T, typename... Args>
    static void FormatStringImpl(std::ostringstream& oss, const std::string& format, T&& value, Args&&... args) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            oss << format.substr(0, pos) << std::forward<T>(value);
            FormatStringImpl(oss, format.substr(pos + 2), std::forward<Args>(args)...);
        } else {
            oss << format;
        }
    }
    
    // 递归终止条件 (Recursion termination condition)
    static void FormatStringImpl(std::ostringstream& oss, const std::string& format) {
        oss << format;
    }
    
    Level current_level_{Level::INFO};  // 当前日志级别 (Current log level)
    bool console_output_{true};         // 是否输出到控制台 (Whether to output to console)
    bool file_output_{false};           // 是否输出到文件 (Whether to output to file)
    std::string log_file_path_;         // 日志文件路径 (Log file path)
    std::ofstream log_file_;            // 日志文件流 (Log file stream)
    mutable std::mutex log_mutex_;      // 日志操作互斥锁 (Log operation mutex)
};

} // namespace tj