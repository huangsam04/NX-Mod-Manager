#pragma once

#include <string>
#include <switch.h>
#include <unordered_map>
#include <memory>

// 全局字符串变量，对应json中的键
// Global string variable corresponding to a key in the JSON
extern std::string LOADING_TEXT;
extern std::string BUTTON_BACK;
extern std::string SOFTWARE_TITLE;
extern std::string SOFTWARE_TITLE_APP;
extern std::string SOFTWARE_TITLE_INSTRUCTION;
extern std::string MOD_COUNT;
extern std::string MOD_VERSION;
extern std::string GAME_VERSION;
extern std::string ABOUT_BUTTON;
extern std::string INSTRUCTION_BUTTON;
extern std::string NO_FOUND_MOD;
extern std::string FPS_TYPE_TEXT;
extern std::string HD_TYPE_TEXT;
extern std::string CHEAT_TYPE_TEXT;
extern std::string COSMETIC_TYPE_TEXT;
extern std::string PLAY_TYPE_TEXT;
extern std::string NONE_TYPE_TEXT;
extern std::string NONE_GAME_TEXT;
extern std::string MOD_TYPE_TEXT;
extern std::string MOD_STATE_INSTALLED;
extern std::string MOD_STATE_UNINSTALLED;
extern std::string VERSION_OK;
extern std::string VERSION_ERROR;
extern std::string VERSION_NONE;
extern std::string MOD_CONTEXT;
extern std::string TOTAL_COUNT;
extern std::string BUTTON_SELECT;
extern std::string BUTTON_EXIT;
extern std::string SORT_ALPHA_AZ;
extern std::string SORT_ALPHA_ZA;
extern std::string FPS_TEXT;
extern std::string HD_TEXT;
extern std::string CHEAT_TEXT;
extern std::string COSMETIC_TEXT;
extern std::string NONE_TEXT;
extern std::string PLAY_TEXT;

// 说明文本 (Instruction texts)
extern std::string INSTRUCTION_LINE_1;
extern std::string INSTRUCTION_LINE_2;
extern std::string INSTRUCTION_LINE_3;
extern std::string INSTRUCTION_LINE_4;
extern std::string INSTRUCTION_LINE_5;
extern std::string INSTRUCTION_LINE_6;
extern std::string INSTRUCTION_LINE_7;
extern std::string INSTRUCTION_LINE_8;
extern std::string INSTRUCTION_LINE_9;
extern std::string INSTRUCTION_LINE_10;
extern std::string INSTRUCTION_LINE_11;
extern std::string INSTRUCTION_LINE_12;
extern std::string INSTRUCTION_LINE_13;
extern std::string INSTRUCTION_LINE_14;
extern std::string INSTRUCTION_LINE_15;
extern std::string INSTRUCTION_LINE_16;
extern std::string INSTRUCTION_LINE_17;
extern std::string INSTRUCTION_LINE_18;
extern std::string INSTRUCTION_LINE_19;
extern std::string INSTRUCTION_LINE_20;
extern std::string INSTRUCTION_LINE_21;
extern std::string INSTRUCTION_LINE_22;
extern std::string INSTRUCTION_LINE_23;
extern std::string INSTRUCTION_LINE_24;


extern std::string CONFIRM_INSTALLED;
extern std::string CONFIRM_UNINSTALLED;
extern std::string SUCCESS_INSTALLED;
extern std::string SUCCESS_UNINSTALLED;
extern std::string FAILURE_INSTALLED;
extern std::string FAILURE_UNINSTALLED;
extern std::string CANCEL_INSTALLED;
extern std::string CANCEL_UNINSTALLED;
extern std::string CANT_OPEN_FILE;
extern std::string CANT_CREATE_DIR;
extern std::string INSTALLEDING_TEXT;
extern std::string UNINSTALLEDING_TEXT;
extern std::string CALCULATE_FILES;
extern std::string COPY_ERROR;
extern std::string UNINSTALLED_ERROR;
extern std::string CLEAR_BUTTON;
extern std::string FILE_NONE;

namespace tj {

class LangManager {

public:
    // 获取单例实例
    // Get the singleton instance
    static LangManager& getInstance();

    // 加载指定语言的JSON文件
    // Load the JSON file for the specified language
    bool loadLanguage(const std::string& langCode);

    // 自动检测并加载系统语言
    // Automatically detect and load the system language
    bool loadSystemLanguage();

    // 获取当前系统语言代号
    // Get the current system language code
    int getCurrentLanguage() const;

    // 加载系统语言
    // Load the system language
    bool loadDefaultLanguage();

    // 禁止拷贝和移动
    // Disable copy and move
    LangManager(const LangManager&) = delete;
    LangManager& operator=(const LangManager&) = delete;
    LangManager(LangManager&&) = delete;
    LangManager& operator=(LangManager&&) = delete;

    

private:
    // 私有构造函数
    // Private constructor
    LangManager() = default;

    // 文本映射表
    // Text mapping table
    std::unordered_map<std::string, std::string> m_textMap;

    // 当前系统语言代号
    // Current system language code
    int m_currentLanguage; 

};

} // namespace tj