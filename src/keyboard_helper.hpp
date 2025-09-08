#pragma once

#include <switch.h>
#include <string>

/**
 * 简单的虚拟键盘辅助函数
 */
namespace KeyboardHelper {
    
    /**
     * 显示虚拟键盘
     * @param title 标题文本
     * @param isNumPad 是否为数字键盘，默认false（普通键盘）
     * @param maxLen 最大长度，默认14
     * @return 输入的文本，取消返回空字符串
     */
    inline std::string showKeyboard(const std::string& title = "", const std::string& context = "",  int maxLen = 14, bool enableNewline = false) {
        SwkbdConfig kbd;
        char textbuf[maxLen*3] = {0};
        
        if (R_FAILED(swkbdCreate(&kbd, 0))) {
            return "";
        }
        
        swkbdConfigSetType(&kbd, SwkbdType_All);
        swkbdConfigSetStringLenMax(&kbd, maxLen);
        
        // 根据参数决定是否启用换行功能
        swkbdConfigSetReturnButtonFlag(&kbd, enableNewline ? 1 : 0);
        
        // 先设置初始文本，再设置光标位置
        swkbdConfigSetHeaderText(&kbd, title.c_str());
        swkbdConfigSetInitialText(&kbd, context.c_str());
        swkbdConfigSetInitialCursorPos(&kbd, 1); // 设置光标在文本最右边 (0=开始, 1=结尾)
        
        
        Result rc = swkbdShow(&kbd, textbuf, sizeof(textbuf));
        swkbdClose(&kbd);
        
        return R_SUCCEEDED(rc) ? std::string(textbuf) : "";
    }


    inline std::string showNumberKeyboard(const std::string& title = "", const std::string& context = "",  int maxLen = 10) {
        SwkbdConfig kbd;
        char textbuf[11] = {0};
        
        if (R_FAILED(swkbdCreate(&kbd, 0))) {
            return "";
        }
        
        swkbdConfigSetType(&kbd, SwkbdType_NumPad);  // 直接使用数字键盘
        swkbdConfigSetStringLenMax(&kbd, maxLen);
        
        // 为数字键盘添加小数点符号
        swkbdConfigSetLeftOptionalSymbolKey(&kbd, ".");
        
        // 先设置初始文本，再设置光标位置
        swkbdConfigSetHeaderText(&kbd, title.c_str());
        swkbdConfigSetInitialText(&kbd, context.c_str());
        swkbdConfigSetInitialCursorPos(&kbd, 1); // 设置光标在文本最右边 (0=开始, 1=结尾)
        
        
        Result rc = swkbdShow(&kbd, textbuf, sizeof(textbuf));
        swkbdClose(&kbd);
        
        return R_SUCCEEDED(rc) ? std::string(textbuf) : "";
    }









}