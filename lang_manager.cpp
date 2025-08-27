#include "lang_manager.hpp"
#include <fstream>
#include <cstring>
#include <string>
#include <functional>

// 定义全局字符串变量
// Global string variables
std::string LOADING_TEXT;
std::string BUTTON_BACK;
std::string SOFTWARE_TITLE;
std::string SOFTWARE_TITLE_APP;
std::string SOFTWARE_TITLE_INSTRUCTION;
std::string MOD_COUNT;
std::string MOD_VERSION;
std::string GAME_VERSION;
std::string ABOUT_BUTTON;
std::string INSTRUCTION_BUTTON;
std::string NO_FOUND_MOD;
std::string FPS_TYPE_TEXT;
std::string HD_TYPE_TEXT;
std::string CHEAT_TYPE_TEXT;
std::string COSMETIC_TYPE_TEXT;
std::string PLAY_TYPE_TEXT;
std::string NONE_TYPE_TEXT;
std::string NONE_GAME_TEXT;
std::string MOD_TYPE_TEXT;
std::string MOD_STATE_INSTALLED;
std::string MOD_STATE_UNINSTALLED;
std::string VERSION_OK;
std::string VERSION_ERROR;
std::string VERSION_NONE;
std::string MOD_CONTEXT;
std::string TOTAL_COUNT;
std::string BUTTON_SELECT;
std::string BUTTON_EXIT;
std::string SORT_ALPHA_AZ;
std::string SORT_ALPHA_ZA;
std::string FPS_TEXT;
std::string HD_TEXT;
std::string CHEAT_TEXT;
std::string COSMETIC_TEXT;
std::string NONE_TEXT;
std::string PLAY_TEXT;

// 说明文本 (Instruction texts)
std::string INSTRUCTION_LINE_1;
std::string INSTRUCTION_LINE_2;
std::string INSTRUCTION_LINE_3;
std::string INSTRUCTION_LINE_4;
std::string INSTRUCTION_LINE_5;
std::string INSTRUCTION_LINE_6;
std::string INSTRUCTION_LINE_7;
std::string INSTRUCTION_LINE_8;
std::string INSTRUCTION_LINE_9;
std::string INSTRUCTION_LINE_10;
std::string INSTRUCTION_LINE_11;
std::string INSTRUCTION_LINE_12;
std::string INSTRUCTION_LINE_13;
std::string INSTRUCTION_LINE_14;
std::string INSTRUCTION_LINE_15;
std::string INSTRUCTION_LINE_16;
std::string INSTRUCTION_LINE_17;
std::string INSTRUCTION_LINE_18;
std::string INSTRUCTION_LINE_19;
std::string INSTRUCTION_LINE_20;
std::string INSTRUCTION_LINE_21;
std::string INSTRUCTION_LINE_22;
std::string INSTRUCTION_LINE_23;
std::string INSTRUCTION_LINE_24;


std::string CONFIRM_INSTALLED;
std::string CONFIRM_UNINSTALLED;
std::string SUCCESS_INSTALLED;
std::string SUCCESS_UNINSTALLED;
std::string FAILURE_INSTALLED;
std::string FAILURE_UNINSTALLED;
std::string CANCEL_INSTALLED;
std::string CANCEL_UNINSTALLED;
std::string CANT_OPEN_FILE;
std::string CANT_CREATE_DIR;
std::string INSTALLEDING_TEXT;
std::string UNINSTALLEDING_TEXT;
std::string CALCULATE_FILES;
std::string COPY_ERROR;
std::string UNINSTALLED_ERROR;
std::string CLEAR_BUTTON;
std::string FILE_NONE;
std::string DNOT_READY;

std::string ZIP_OPEN_ERROR;
std::string ZIP_READ_ERROR;
std::string CANT_READ_ERROR;
std::string CANT_CREATE_ERROR;
std::string CANT_WRITE_ERROR;

namespace tj {

LangManager& LangManager::getInstance() {
    static LangManager instance;
    return instance;
}

// 改进的JSON解析函数，能够处理空格和换行符
// Improved JSON parsing function that can handle whitespace and line breaks
bool parseSimpleJSON(const std::string& jsonStr, std::unordered_map<std::string, std::string>& textMap) {
    textMap.clear();
    size_t pos = jsonStr.find('{'); // 定位到JSON对象开始
    if (pos == std::string::npos) return false;
    pos++;

    // 跳过空格和换行符的辅助函数
    // Helper function to skip whitespace and line breaks
    auto skipWhitespace = [&](size_t start) {
        while (start < jsonStr.size() && (jsonStr[start] == ' ' || jsonStr[start] == '\t' || jsonStr[start] == '\n' || jsonStr[start] == '\r')) {
            start++;
        }
        return start;
    };

    // 查找所有键值对
    // Find all key-value pairs
    while (pos < jsonStr.size()) {
        pos = skipWhitespace(pos);
        if (pos >= jsonStr.size() || jsonStr[pos] == '}') break;

        // 查找键的开始（双引号）
        // Find the start of the key (double quote)
        if (jsonStr[pos] != '"') {
            // 不是双引号，可能是格式错误，尝试找到下一个双引号
            // It's not a double quote. It might be a formatting error. Please try to find the next double quote.
            pos = jsonStr.find('"', pos);
            if (pos == std::string::npos || jsonStr[pos] != '"') {
                // 找不到双引号，解析失败
                // Can't find a double quote. Parsing failed.
                break;
            }
        }
        size_t keyStart = pos + 1;

        // 查找键的结束（双引号，考虑转义字符）
        // Find the end of the key (double quote, considering escape characters)
        bool inEscape = false;
        size_t keyEnd = keyStart;
        while (keyEnd < jsonStr.size()) {
            if (!inEscape && jsonStr[keyEnd] == '\\') {
                inEscape = true;
            } else if (!inEscape && jsonStr[keyEnd] == '"') {
                break;
            } else {
                inEscape = false;
            }
            keyEnd++;
        }
        if (keyEnd >= jsonStr.size()) break;

        // 提取键
        // Extract the key
        std::string key = jsonStr.substr(keyStart, keyEnd - keyStart);

        // 移动到冒号
        // Move to the colon
        pos = keyEnd + 1;
        pos = skipWhitespace(pos);
        if (pos >= jsonStr.size() || jsonStr[pos] != ':') {
            // 找不到冒号，跳过到下一个逗号或右括号
            // Can't find a colon. Skip to the next comma or right parenthesis.
            pos = jsonStr.find_first_of(",}", pos);
            if (pos < jsonStr.size() && jsonStr[pos] == ',') pos++;
            continue;
        }
        pos++;
        pos = skipWhitespace(pos);

        // 查找值的开始（双引号）
        // Find the start of the value (double quote)
        if (pos >= jsonStr.size() || jsonStr[pos] != '"') {
            // 找不到双引号，跳过到下一个逗号或右括号
            // Can't find a double quote. Skip to the next comma or right parenthesis.
            pos = jsonStr.find_first_of(",}", pos);
            if (pos < jsonStr.size() && jsonStr[pos] == ',') pos++;
            continue;
        }
        size_t valStart = pos + 1;

        // 查找值的结束（双引号，考虑转义字符）
        // Find the end of the value (double quote, considering escape characters)
        inEscape = false;
        size_t valEnd = valStart;
        while (valEnd < jsonStr.size()) {
            if (!inEscape && jsonStr[valEnd] == '\\') {
                inEscape = true;
            } else if (!inEscape && jsonStr[valEnd] == '"') {
                break;
            } else {
                inEscape = false;
            }
            valEnd++;
        }
        if (valEnd >= jsonStr.size()) break;

        // 提取值并处理转义字符
        // Extract the value and handle escape characters
        std::string value = jsonStr.substr(valStart, valEnd - valStart);
        std::string processedValue;
        for (size_t i = 0; i < value.size(); i++) {
            if (value[i] == '\\' && i + 1 < value.size()) {
                i++;
                switch (value[i]) {
                    case 'n': processedValue += '\n'; break;
                    case 't': processedValue += '\t'; break;
                    case 'r': processedValue += '\r'; break;
                    case '"': processedValue += '"'; break;
                    case '\\': processedValue += '\\'; break;
                    default: processedValue += value[i]; break;
                }
            } else {
                processedValue += value[i];
            }
        }

        textMap[key] = processedValue;

        // 移动到下一个键值对
        // Move to the next key-value pair
        pos = valEnd + 1;
        pos = skipWhitespace(pos);
        if (pos < jsonStr.size() && jsonStr[pos] == ',') pos++;
    }

    return !textMap.empty();
}

bool LangManager::loadLanguage(const std::string& langCode) {
    // 清空现有文本映射
    // Clear existing text mappings
    m_textMap.clear();

    // 构建语言文件路径
    // Build the language file path
    std::string filePath = "romfs:/lang/" + langCode + ".json";

    // 尝试打开文件
    // Try to open the file
    std::ifstream file(filePath);
    if (!file.is_open()) {
        // 如果指定语言文件不存在，尝试加载en.json
        // If the specified language file doesn't exist, try to load en.json
        filePath = "romfs:/lang/en.json";
        file.open(filePath);
        if (!file.is_open()) {
            return false;
        }
    }

    // 读取整个文件内容
    // Read the entire file content
    std::string jsonStr((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // 解析JSON
    // Parse the JSON
    bool parseSuccess = parseSimpleJSON(jsonStr, m_textMap);

    // 如果解析成功，更新全局变量
    // If the parsing is successful, update the global variables
    if (parseSuccess) {
        // 创建键与变量的映射关系
        // Create a mapping relationship between keys and variables
        std::vector<std::pair<std::string, std::reference_wrapper<std::string>>> textMappings = {
            {"LOADING_TEXT", std::ref(LOADING_TEXT)},
            {"BUTTON_BACK", std::ref(BUTTON_BACK)},
            {"SOFTWARE_TITLE", std::ref(SOFTWARE_TITLE)},
            {"SOFTWARE_TITLE_APP", std::ref(SOFTWARE_TITLE_APP)},
            {"SOFTWARE_TITLE_INSTRUCTION", std::ref(SOFTWARE_TITLE_INSTRUCTION)},
            {"MOD_COUNT", std::ref(MOD_COUNT)},
            {"MOD_VERSION", std::ref(MOD_VERSION)},
            {"GAME_VERSION", std::ref(GAME_VERSION)},
            {"ABOUT_BUTTON", std::ref(ABOUT_BUTTON)},
            {"INSTRUCTION_BUTTON", std::ref(INSTRUCTION_BUTTON)},
            {"NO_FOUND_MOD", std::ref(NO_FOUND_MOD)},
            {"FPS_TYPE_TEXT", std::ref(FPS_TYPE_TEXT)},
            {"HD_TYPE_TEXT", std::ref(HD_TYPE_TEXT)},
            {"CHEAT_TYPE_TEXT", std::ref(CHEAT_TYPE_TEXT)},
            {"COSMETIC_TYPE_TEXT", std::ref(COSMETIC_TYPE_TEXT)},
            {"PLAY_TYPE_TEXT", std::ref(PLAY_TYPE_TEXT)},
            {"NONE_TYPE_TEXT", std::ref(NONE_TYPE_TEXT)},
            {"NONE_GAME_TEXT", std::ref(NONE_GAME_TEXT)},
            {"MOD_TYPE_TEXT", std::ref(MOD_TYPE_TEXT)},
            {"MOD_STATE_INSTALLED", std::ref(MOD_STATE_INSTALLED)},
            {"MOD_STATE_UNINSTALLED", std::ref(MOD_STATE_UNINSTALLED)},
            {"VERSION_OK", std::ref(VERSION_OK)},
            {"VERSION_ERROR", std::ref(VERSION_ERROR)},
            {"VERSION_NONE", std::ref(VERSION_NONE)},
            {"MOD_CONTEXT", std::ref(MOD_CONTEXT)},
            {"TOTAL_COUNT", std::ref(TOTAL_COUNT)},
            {"BUTTON_SELECT", std::ref(BUTTON_SELECT)},
            {"BUTTON_EXIT", std::ref(BUTTON_EXIT)},
            {"SORT_ALPHA_AZ", std::ref(SORT_ALPHA_AZ)},
            {"SORT_ALPHA_ZA", std::ref(SORT_ALPHA_ZA)},
            {"FPS_TEXT", std::ref(FPS_TEXT)},
            {"HD_TEXT", std::ref(HD_TEXT)},
            {"CHEAT_TEXT", std::ref(CHEAT_TEXT)},
            {"COSMETIC_TEXT", std::ref(COSMETIC_TEXT)},
            {"NONE_TEXT", std::ref(NONE_TEXT)},
            {"PLAY_TEXT", std::ref(PLAY_TEXT)},
            {"INSTRUCTION_LINE_1", std::ref(INSTRUCTION_LINE_1)},
            {"INSTRUCTION_LINE_2", std::ref(INSTRUCTION_LINE_2)},
            {"INSTRUCTION_LINE_3", std::ref(INSTRUCTION_LINE_3)},
            {"INSTRUCTION_LINE_4", std::ref(INSTRUCTION_LINE_4)},
            {"INSTRUCTION_LINE_5", std::ref(INSTRUCTION_LINE_5)},
            {"INSTRUCTION_LINE_6", std::ref(INSTRUCTION_LINE_6)},
            {"INSTRUCTION_LINE_7", std::ref(INSTRUCTION_LINE_7)},
            {"INSTRUCTION_LINE_8", std::ref(INSTRUCTION_LINE_8)},
            {"INSTRUCTION_LINE_9", std::ref(INSTRUCTION_LINE_9)},
            {"INSTRUCTION_LINE_10", std::ref(INSTRUCTION_LINE_10)},
            {"INSTRUCTION_LINE_11", std::ref(INSTRUCTION_LINE_11)},
            {"INSTRUCTION_LINE_12", std::ref(INSTRUCTION_LINE_12)},
            {"INSTRUCTION_LINE_13", std::ref(INSTRUCTION_LINE_13)},
            {"INSTRUCTION_LINE_14", std::ref(INSTRUCTION_LINE_14)},
            {"INSTRUCTION_LINE_15", std::ref(INSTRUCTION_LINE_15)},
            {"INSTRUCTION_LINE_16", std::ref(INSTRUCTION_LINE_16)},
            {"INSTRUCTION_LINE_17", std::ref(INSTRUCTION_LINE_17)},
            {"INSTRUCTION_LINE_18", std::ref(INSTRUCTION_LINE_18)},
            {"INSTRUCTION_LINE_19", std::ref(INSTRUCTION_LINE_19)},
            {"INSTRUCTION_LINE_20", std::ref(INSTRUCTION_LINE_20)},
            {"INSTRUCTION_LINE_21", std::ref(INSTRUCTION_LINE_21)},
            {"INSTRUCTION_LINE_22", std::ref(INSTRUCTION_LINE_22)},
            {"INSTRUCTION_LINE_23", std::ref(INSTRUCTION_LINE_23)},
            {"INSTRUCTION_LINE_24", std::ref(INSTRUCTION_LINE_24)},

            
            {"CONFIRM_INSTALLED", std::ref(CONFIRM_INSTALLED)},
            {"CONFIRM_UNINSTALLED", std::ref(CONFIRM_UNINSTALLED)},
            {"SUCCESS_INSTALLED", std::ref(SUCCESS_INSTALLED)},
            {"SUCCESS_UNINSTALLED", std::ref(SUCCESS_UNINSTALLED)},
            {"FAILURE_INSTALLED", std::ref(FAILURE_INSTALLED)},
            {"FAILURE_UNINSTALLED", std::ref(FAILURE_UNINSTALLED)},
            {"CANCEL_INSTALLED", std::ref(CANCEL_INSTALLED)},
            {"CANCEL_UNINSTALLED", std::ref(CANCEL_UNINSTALLED)},
            {"CANT_OPEN_FILE", std::ref(CANT_OPEN_FILE)},
            {"CANT_CREATE_DIR", std::ref(CANT_CREATE_DIR)},
            {"INSTALLEDING_TEXT", std::ref(INSTALLEDING_TEXT)},
            {"UNINSTALLEDING_TEXT", std::ref(UNINSTALLEDING_TEXT)},
            {"CALCULATE_FILES", std::ref(CALCULATE_FILES)},
            {"COPY_ERROR", std::ref(COPY_ERROR)},
            {"UNINSTALLED_ERROR", std::ref(UNINSTALLED_ERROR)},
            {"CLEAR_BUTTON", std::ref(CLEAR_BUTTON)},
            {"FILE_NONE", std::ref(FILE_NONE)},
            {"DNOT_READY", std::ref(DNOT_READY)},

            {"ZIP_OPEN_ERROR", std::ref(ZIP_OPEN_ERROR)},
            {"ZIP_READ_ERROR", std::ref(ZIP_READ_ERROR)},
            {"CANT_READ_ERROR", std::ref(CANT_READ_ERROR)},
            {"CANT_CREATE_ERROR", std::ref(CANT_CREATE_ERROR)},
            {"CANT_WRITE_ERROR", std::ref(CANT_WRITE_ERROR)},
        };

        // 遍历映射表进行赋值 - 修改为更兼容的遍历方式
        // Traverse the mapping table to assign values - modify to a more compatible traversal way
        for (size_t i = 0; i < textMappings.size(); ++i) {
            const std::string& key = textMappings[i].first;
            std::reference_wrapper<std::string>& ref = textMappings[i].second;
            
            auto it = m_textMap.find(key);
            if (it != m_textMap.end()) {
                ref.get() = it->second;  // 显式使用get()获取引用 // Explicitly use get() to obtain a reference
                
                
            } 
        }
    }
    
    return parseSuccess;
}

int LangManager::getCurrentLanguage() const {


    return m_currentLanguage; 
}


bool LangManager::loadSystemLanguage() {
    u64 LanguageCode = 0;
    SetLanguage Language = SetLanguage_ENUS;
    Result rc = setInitialize();
    
    if (R_SUCCEEDED(rc)) {
        rc = setGetSystemLanguage(&LanguageCode);
        if (R_SUCCEEDED(rc)) {
            rc = setMakeLanguage(LanguageCode, &Language);
        }
        setExit(); // 确保关闭set服务 // Ensure that the set service is closed
    }
    
    if (R_SUCCEEDED(rc)) {
        // 根据系统语言加载对应的语言文件
        // Load the corresponding language file based on the system language
        m_currentLanguage = Language;
        switch(Language) {
            case 15: // 简体中文
                m_currentLanguage = 14;
                return loadLanguage("zh-Hans");
            case 16: // 繁体中文
                m_currentLanguage = 13;
                return loadLanguage("zh-Hant");
            case 0: // 日语
                m_currentLanguage = 2;
                return loadLanguage("ja");
            case 7: // 韩语
                m_currentLanguage = 12;
                return loadLanguage("ko");
            case 2: // 法语
                m_currentLanguage = 3;
                return loadLanguage("fr");
            case 3: // 德语
                m_currentLanguage = 4;  
                return loadLanguage("de");
            case 10: // 俄语
                m_currentLanguage = 11;
                return loadLanguage("ru");
            case 5: // 西班牙语
                m_currentLanguage = 5;
                return loadLanguage("es");
            case 9: // 葡萄牙语
                m_currentLanguage = 15;
                return loadLanguage("pt");
            case 4: // 意大利语
                m_currentLanguage = 7;
                return loadLanguage("it");
            case 8: // 荷兰语
                m_currentLanguage = 8;
                return loadLanguage("nl");
            default: // 默认英语
                m_currentLanguage = 0;
                return loadLanguage("en");
        }
    } else {
        // 如果获取系统语言失败，默认加载英语
        // If the system language cannot be obtained, load English by default
        return loadLanguage("en");
    }
}



} // namespace tj
