#include <switch.h>
#include "app.hpp"
#include "utils/logger.hpp"

int main(int argc, char* argv[]) {
    // 初始化系统服务
    consoleInit(nullptr);
    
    // 设置日志级别
    tj::Logger::SetLevel(tj::Logger::Level::INFO);
    
    // 创建应用程序实例
    tj::App app;
    
    // 运行主循环
    app.Loop();
    
    // 清理资源
    consoleExit(nullptr);
    
    return 0;
}