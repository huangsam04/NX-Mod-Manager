#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "yyjson/yyjson.h"

namespace tj {

/**
 * JSON管理器类 - 封装所有JSON文件操作
 * JSON Manager Class - Encapsulates all JSON file operations
 */
class JsonManager {
public:
    /**
     * 确保JSON文件存在，如果不存在则创建空的JSON对象文件
     * Ensure JSON file exists, create empty JSON object file if not exists
     * @param json_path JSON文件路径 (JSON file path)
     * @return 成功返回true，失败返回false (Returns true on success, false on failure)
     */
    static bool EnsureJsonFileExists(const std::string& json_path);

    /**
     * 删除JSON文件中嵌套的指定键值对
     * Remove nested key-value pair from JSON file
     * @param json_path JSON文件路径 (JSON file path)
     * @param root_key 根键名 (Root key name)
     * @param key 要删除的嵌套键名 (Nested key name to remove)
     * @return 成功返回true，失败返回false (Returns true on success, false on failure)
     */
    static bool RemoveNestedJsonKeyValue(const std::string& json_path, const std::string& root_key, const std::string& key);

    /**
     * 删除JSON文件中指定的根级键值对
     * Remove root-level key-value pair from JSON file
     * @param json_path JSON文件路径 (JSON file path)
     * @param key 要删除的键名 (Key name to remove)
     * @return 成功返回true，失败返回false (Returns true on success, false on failure)
     */
    static bool RemoveRootJsonKeyValue(const std::string& json_path, const std::string& key);

    /**
     * 更新或添加JSON文件中的根级键值对
     * Update or add root-level key-value pair in JSON file
     * @param json_path JSON文件路径 (JSON file path)
     * @param key 键名 (Key name)
     * @param value 键值 (Key value)
     * @return 成功返回true，失败返回false (Returns true on success, false on failure)
     */
    static bool UpdateRootJsonKeyValue(const std::string& json_path, const std::string& key, const std::string& value);

    /**
     * 向嵌套的JSON结构中添加或更新键值对
     * Add or update key-value pair in nested JSON structure
     * @param json_path JSON文件路径 (JSON file path)
     * @param root_key 根键名 (Root key name)
     * @param nested_key 嵌套键名 (Nested key name)
     * @param nested_value 嵌套键值 (Nested key value)
     * @return 成功返回true，失败返回false (Returns true on success, false on failure)
     */
    static bool UpdateNestedJsonKeyValue(const std::string& json_path, const std::string& root_key, 
                                         const std::string& nested_key, const std::string& nested_value);

    /**
     * 重命名根键键名，未找到根键时直接报错
     * @param json_path JSON文件路径 (JSON file path)
     * @param old_root_key 旧根键名 (Old root key name)
     * @param new_root_key 新根键名 (New root key name)
     * @return 成功返回true，失败返回false (Returns true on success, false on failure)
     */
    static bool RenameJsonRootKey(const std::string& json_path, const std::string& old_root_key, const std::string& new_root_key);

    /**
     * 重命名根键键名，如果旧根键不存在则创建新的空根键对象
     * Rename root key, create new empty root key object if old root key doesn't exist
     * @param json_path JSON文件路径 (JSON file path)
     * @param old_root_key 旧根键名 (Old root key name)
     * @param new_root_key 新根键名 (New root key name)
     * @return 成功返回true，失败返回false (Returns true on success, false on failure)
     */
    static bool RenameOrCreateJsonRootKey(const std::string& json_path, const std::string& old_root_key, const std::string& new_root_key);

    /**
     * 获取根级JSON键的值，如果键不存在则返回键名本身
     * Get root-level JSON key value, return key name if key doesn't exist
     * @param json_path JSON文件路径 (JSON file path)
     * @param key 键名 (Key name)
     * @return 键值或键名 (Key value or key name)
     */
    static std::string GetRootJsonValue(const std::string& json_path, const std::string& key);

    /**
     * 获取嵌套JSON键的值，如果根键或嵌套键不存在则返回根键
     * Get nested JSON key value, return root key if root key or nested key doesn't exist
     * @param json_path JSON文件路径 (JSON file path)
     * @param root_key 根键名 (Root key name)
     * @param nested_key 嵌套键名 (Nested key name)
     * @return 嵌套键的值或根键名 (Nested key value or root key name)
     */
    static std::string GetNestedJsonValue(const std::string& json_path, const std::string& root_key, const std::string& nested_key);

    

    /**
     * 读取并解析JSON文件内容
     * Read and parse JSON file content
     * @param json_path JSON文件路径 (JSON file path)
     * @param out_doc 输出的JSON文档指针 (Output JSON document pointer)
     * @return 成功返回true，失败返回false (Returns true on success, false on failure)
     */
    static bool ReadJsonFile(const std::string& json_path, yyjson_doc** out_doc);

    /**
     * 将JSON文档写入文件
     * Write JSON document to file
     * @param json_path JSON文件路径 (JSON file path)
     * @param doc JSON文档指针 (JSON document pointer)
     * @param pretty_format 是否格式化输出 (Whether to format output)
     * @return 成功返回true，失败返回false (Returns true on success, false on failure)
     */
    static bool WriteJsonFile(const std::string& json_path, yyjson_mut_doc* mut_doc, bool pretty_format = true);

private:
    // 私有构造函数，防止实例化 (Private constructor to prevent instantiation)
    JsonManager() = delete;
    ~JsonManager() = delete;
    JsonManager(const JsonManager&) = delete;
    JsonManager& operator=(const JsonManager&) = delete;
};

} // namespace tj