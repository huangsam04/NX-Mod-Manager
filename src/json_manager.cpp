#include "json_manager.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <sys/stat.h>

namespace tj {

// 确保JSON文件存在，如果不存在则创建空的JSON对象文件
// Ensure JSON file exists, create empty JSON object file if not exists
bool JsonManager::EnsureJsonFileExists(const std::string& json_path) {
    // 检查文件是否存在 (Check if file exists)
    struct stat buffer;
    if (stat(json_path.c_str(), &buffer) == 0) {
        return true; // 文件已存在 (File already exists)
    }
    
    // 文件不存在，创建空的JSON对象文件 (File doesn't exist, create empty JSON object file)
    FILE* file = fopen(json_path.c_str(), "w");
    if (!file) {
        return false; // 无法创建文件 (Cannot create file)
    }
    
    // 写入空的JSON对象 (Write empty JSON object)
    const char* empty_json = "{\n}\n";
    size_t written = fwrite(empty_json, 1, strlen(empty_json), file);
    fclose(file);
    
    return written == strlen(empty_json);
}

// 删除JSON文件中嵌套的指定键值对（如果不存在也返回true）
// Remove nested key-value pair from JSON file (returns true if key doesn't exist)
bool JsonManager::RemoveNestedJsonKeyValue(const std::string& json_path, const std::string& root_key, const std::string& key) {
    // 确保JSON文件存在 (Ensure JSON file exists)
    if (!EnsureJsonFileExists(json_path)) return false;
    
    // 打开文件进行读取 (Open file for reading)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) return false;
    
    // 获取文件大小 (Get file size)
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 读取文件内容 (Read file content)
    char* json_data = (char*)malloc(file_size + 1);
    if (!json_data) {
        fclose(file);
        return false;
    }
    
    fread(json_data, 1, file_size, file);
    json_data[file_size] = '\0';
    fclose(file);
    
    // 解析JSON (Parse JSON)
    yyjson_doc* doc = yyjson_read(json_data, file_size, 0);
    free(json_data);
    if (!doc) return false;
    
    // 创建可变文档 (Create mutable document)
    yyjson_mut_doc* mut_doc = yyjson_doc_mut_copy(doc, NULL);
    yyjson_doc_free(doc);
    if (!mut_doc) return false;
    
    yyjson_mut_val* root = yyjson_mut_doc_get_root(mut_doc);
    if (!root || !yyjson_mut_is_obj(root)) {
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 查找根键对应的对象 (Find object for root key)
    yyjson_mut_val* root_obj = yyjson_mut_obj_get(root, root_key.c_str());
    if (!root_obj || !yyjson_mut_is_obj(root_obj)) {
        // 根键不存在或不是对象，无法删除嵌套键 (Root key doesn't exist or is not an object, cannot remove nested key)
        yyjson_mut_doc_free(mut_doc);
        return true;
    }
    
    // 从根键对象中删除指定的嵌套键 (Remove specified nested key from root key object)
    yyjson_mut_val* removed_val = yyjson_mut_obj_remove_str(root_obj, key.c_str());
    if (!removed_val) {
        // 嵌套键不存在 (Nested key doesn't exist)
        yyjson_mut_doc_free(mut_doc);
        return true;
    }
    
    // 写入文件 (Write to file)
    return WriteJsonFile(json_path, mut_doc, true);
}

// 删除JSON文件中指定的根级键值对（如果不存在也返回true）
// Remove root-level key-value pair from JSON file (returns true if key doesn't exist)
bool JsonManager::RemoveRootJsonKeyValue(const std::string& json_path, const std::string& key) {
    // 确保JSON文件存在 (Ensure JSON file exists)
    if (!EnsureJsonFileExists(json_path)) return false;
    
    // 打开文件进行读取 (Open file for reading)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) return false;
    
    // 获取文件大小 (Get file size)
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 读取文件内容 (Read file content)
    std::vector<char> buffer(file_size + 1);
    size_t read_size = fread(buffer.data(), 1, file_size, file);
    fclose(file);
    
    if (read_size != file_size) return false;
    buffer[file_size] = '\0';
    
    // 解析JSON (Parse JSON)
    yyjson_doc* doc = yyjson_read(buffer.data(), file_size, 0);
    if (!doc) return false;
    
    // 创建可变文档 (Create mutable document)
    yyjson_mut_doc* mut_doc = yyjson_doc_mut_copy(doc, NULL);
    yyjson_doc_free(doc);
    if (!mut_doc) return false;
    
    // 获取根对象 (Get root object)
    yyjson_mut_val* root = yyjson_mut_doc_get_root(mut_doc);
    if (!root || !yyjson_mut_is_obj(root)) {
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 删除指定的键值对 (Remove the specified key-value pair)
    yyjson_mut_val* removed_val = yyjson_mut_obj_remove_str(root, key.c_str());
    
    // 如果键不存在，返回true (Return true if key doesn't exist)
    if (!removed_val) {
        yyjson_mut_doc_free(mut_doc);
        return true;
    }
    
    // 写入文件 (Write to file)
    return WriteJsonFile(json_path, mut_doc, true);
}

// 更新或添加JSON文件中的根级键值对
// Update or add root-level key-value pair in JSON file
bool JsonManager::UpdateRootJsonKeyValue(const std::string& json_path, const std::string& key, const std::string& value) {
    // 确保JSON文件存在 (Ensure JSON file exists)
    if (!EnsureJsonFileExists(json_path)) return false;
    
    // 打开文件进行读取 (Open file for reading)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) return false;
    
    // 获取文件大小 (Get file size)
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 读取文件内容 (Read file content)
    char* json_data = (char*)malloc(file_size + 1);
    if (!json_data) {
        fclose(file);
        return false;
    }
    
    fread(json_data, 1, file_size, file);
    json_data[file_size] = '\0';
    fclose(file);
    
    // 解析JSON (Parse JSON)
    yyjson_doc* doc = yyjson_read(json_data, file_size, 0);
    free(json_data);
    if (!doc) return false;
    
    // 创建可变文档 (Create mutable document)
    yyjson_mut_doc* mut_doc = yyjson_doc_mut_copy(doc, NULL);
    yyjson_doc_free(doc);
    if (!mut_doc) return false;
    
    yyjson_mut_val* root = yyjson_mut_doc_get_root(mut_doc);
    if (!root || !yyjson_mut_is_obj(root)) {
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 替换或添加键值对 (Replace or add key-value pair)
    yyjson_mut_val* key_val = yyjson_mut_str(mut_doc, key.c_str());
    yyjson_mut_val* val_val = yyjson_mut_str(mut_doc, value.c_str());
    if (!key_val || !val_val || !yyjson_mut_obj_put(root, key_val, val_val)) {
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 写入文件 (Write to file)
    return WriteJsonFile(json_path, mut_doc, true);
}

// MOD专用：处理嵌套JSON结构的键值对修改
// MOD-specific: Handle nested JSON structure key-value modifications
bool JsonManager::UpdateNestedJsonKeyValue(const std::string& json_path, const std::string& root_key, 
                                           const std::string& nested_key, const std::string& nested_value) {
    // 确保JSON文件存在 (Ensure JSON file exists)
    if (!EnsureJsonFileExists(json_path)) return false;
    
    // 打开文件进行读取 (Open file for reading)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) return false;
    
    // 获取文件大小 (Get file size)
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 读取文件内容 (Read file content)
    char* json_data = (char*)malloc(file_size + 1);
    if (!json_data) {
        fclose(file);
        return false;
    }
    
    fread(json_data, 1, file_size, file);
    json_data[file_size] = '\0';
    fclose(file);
    
    // 解析JSON (Parse JSON)
    yyjson_doc* doc = yyjson_read(json_data, file_size, 0);
    free(json_data);
    if (!doc) return false;
    
    // 创建可变文档 (Create mutable document)
    yyjson_mut_doc* mut_doc = yyjson_doc_mut_copy(doc, NULL);
    yyjson_doc_free(doc);
    if (!mut_doc) return false;
    
    yyjson_mut_val* root = yyjson_mut_doc_get_root(mut_doc);
    if (!root || !yyjson_mut_is_obj(root)) {
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 查找或创建根键对应的对象 (Find or create object for root key)
    yyjson_mut_val* root_obj = yyjson_mut_obj_get(root, root_key.c_str());
    if (!root_obj) {
        // 根键不存在，创建新的嵌套对象 (Root key doesn't exist, create new nested object)
        root_obj = yyjson_mut_obj(mut_doc);
        if (!root_obj) {
            yyjson_mut_doc_free(mut_doc);
            return false;
        }
        
        yyjson_mut_val* root_key_val = yyjson_mut_str(mut_doc, root_key.c_str());
        if (!root_key_val) {
            yyjson_mut_doc_free(mut_doc);
            return false;
        }
        
        // 如果是favorites$180根键，插入到JSON对象的开头位置
        // If it's favorites$180 root key, insert at the beginning of JSON object
        if (root_key == "favorites$180") {
            if (!yyjson_mut_obj_insert(root, root_key_val, root_obj, 0)) {
                yyjson_mut_doc_free(mut_doc);
                return false;
            }
        } else {
            // 其他根键按原来的方式添加到末尾 (Other root keys are added at the end as before)
            if (!yyjson_mut_obj_put(root, root_key_val, root_obj)) {
                yyjson_mut_doc_free(mut_doc);
                return false;
            }
        }
    } else if (!yyjson_mut_is_obj(root_obj)) {
        // 根键存在但不是对象，需要替换为对象 (Root key exists but is not an object, need to replace with object)
        root_obj = yyjson_mut_obj(mut_doc);
        if (!root_obj) {
            yyjson_mut_doc_free(mut_doc);
            return false;
        }
        
        yyjson_mut_val* root_key_val = yyjson_mut_str(mut_doc, root_key.c_str());
        if (!root_key_val || !yyjson_mut_obj_put(root, root_key_val, root_obj)) {
            yyjson_mut_doc_free(mut_doc);
            return false;
        }
    }
    
    // 在根键对象中设置嵌套键值对 (Set nested key-value pair in root key object)
    yyjson_mut_val* nested_key_val = yyjson_mut_str(mut_doc, nested_key.c_str());
    yyjson_mut_val* nested_val_val = yyjson_mut_str(mut_doc, nested_value.c_str());
    if (!nested_key_val || !nested_val_val || !yyjson_mut_obj_put(root_obj, nested_key_val, nested_val_val)) {
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 写入文件 (Write to file)
    return WriteJsonFile(json_path, mut_doc, true);
}

// MOD专用：支持根键重命名的JSON文件更新操作
// MOD-specific: JSON file update operation with root key renaming support
bool JsonManager::RenameJsonRootKey(const std::string& json_path, const std::string& old_root_key, const std::string& new_root_key) {
    // 确保JSON文件存在 (Ensure JSON file exists)
    if (!EnsureJsonFileExists(json_path)) return false;
    
    // 打开文件进行读取 (Open file for reading)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) return false;
    
    // 获取文件大小 (Get file size)
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 读取文件内容 (Read file content)
    char* json_data = (char*)malloc(file_size + 1);
    if (!json_data) {
        fclose(file);
        return false;
    }
    
    fread(json_data, 1, file_size, file);
    json_data[file_size] = '\0';
    fclose(file);
    
    // 解析JSON (Parse JSON)
    yyjson_doc* doc = yyjson_read(json_data, file_size, 0);
    free(json_data);
    if (!doc) return false;
    
    // 创建可变文档 (Create mutable document)
    yyjson_mut_doc* mut_doc = yyjson_doc_mut_copy(doc, NULL);
    yyjson_doc_free(doc);
    if (!mut_doc) return false;
    
    yyjson_mut_val* root = yyjson_mut_doc_get_root(mut_doc);
    if (!root || !yyjson_mut_is_obj(root)) {
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 查找旧根键 (Find old root key)
    yyjson_mut_val* old_value = yyjson_mut_obj_get(root, old_root_key.c_str());
    if (!old_value) {
        // 旧根键不存在 (Old root key doesn't exist)
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 创建新根键的副本 (Create copy for new root key)
    yyjson_mut_val* new_key_val = yyjson_mut_str(mut_doc, new_root_key.c_str());
    if (!new_key_val) {
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 添加新键值对 (Add new key-value pair)
    if (!yyjson_mut_obj_put(root, new_key_val, old_value)) {
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 删除旧键 (Remove old key)
    yyjson_mut_obj_remove_str(root, old_root_key.c_str());
    
    // 写入文件 (Write to file)
    return WriteJsonFile(json_path, mut_doc, true);
}

// 重命名根键键名，如果旧根键不存在则创建新的空根键对象
// Rename root key, create new empty root key object if old root key doesn't exist
bool JsonManager::RenameOrCreateJsonRootKey(const std::string& json_path, const std::string& old_root_key, const std::string& new_root_key) {
    // 确保JSON文件存在 (Ensure JSON file exists)
    if (!EnsureJsonFileExists(json_path)) return false;
    
    // 打开文件进行读取 (Open file for reading)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) return false;
    
    // 获取文件大小 (Get file size)
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 读取文件内容 (Read file content)
    char* json_data = (char*)malloc(file_size + 1);
    if (!json_data) {
        fclose(file);
        return false;
    }
    
    fread(json_data, 1, file_size, file);
    json_data[file_size] = '\0';
    fclose(file);
    
    // 解析JSON (Parse JSON)
    yyjson_doc* doc = yyjson_read(json_data, file_size, 0);
    free(json_data);
    if (!doc) return false;
    
    // 创建可变文档 (Create mutable document)
    yyjson_mut_doc* mut_doc = yyjson_doc_mut_copy(doc, NULL);
    yyjson_doc_free(doc);
    if (!mut_doc) return false;
    
    yyjson_mut_val* root = yyjson_mut_doc_get_root(mut_doc);
    if (!root || !yyjson_mut_is_obj(root)) {
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 检查旧根键是否存在 (Check if old root key exists)
    yyjson_mut_val* old_root_obj = yyjson_mut_obj_get(root, old_root_key.c_str());
    
    if (old_root_obj) {
        // 旧根键存在，使用yyjson_mut_obj_rename_key直接重命名
        // Old root key exists, use yyjson_mut_obj_rename_key to rename directly
        if (!yyjson_mut_obj_rename_key(mut_doc, root, old_root_key.c_str(), new_root_key.c_str())) {
            yyjson_mut_doc_free(mut_doc);
            return false;
        }
    } else {
        // 旧根键不存在，创建新根键 (Old root key doesn't exist, create new root key)
        yyjson_mut_val* new_root_obj = yyjson_mut_obj(mut_doc);
        if (!new_root_obj) {
            yyjson_mut_doc_free(mut_doc);
            return false;
        }
        
        yyjson_mut_val* new_root_key_val = yyjson_mut_str(mut_doc, new_root_key.c_str());
        if (!new_root_key_val || !yyjson_mut_obj_put(root, new_root_key_val, new_root_obj)) {
            yyjson_mut_doc_free(mut_doc);
            return false;
        }
    }
    
    // 写入文件 (Write to file)
    return WriteJsonFile(json_path, mut_doc, true);
}

// 读取并解析JSON文件内容
// Read and parse JSON file content
bool JsonManager::ReadJsonFile(const std::string& json_path, yyjson_doc** out_doc) {
    if (!out_doc) return false;
    
    // 打开文件 (Open file)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) return false;
    
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
    *out_doc = yyjson_read(buffer.data(), file_size, 0);
    return (*out_doc != nullptr);
}

// 将JSON文档写入文件
// Write JSON document to file
bool JsonManager::WriteJsonFile(const std::string& json_path, yyjson_mut_doc* mut_doc, bool pretty_format) {
    if (!mut_doc) return false;
    
    // 生成JSON字符串 (Generate JSON string)
    yyjson_write_flag flag = pretty_format ? YYJSON_WRITE_PRETTY : 0;
    const char* json_str = yyjson_mut_write(mut_doc, flag, NULL);
    if (!json_str) {
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    // 写入文件 (Write to file)
    FILE* out_file = fopen(json_path.c_str(), "wb");
    if (!out_file) {
        free((void*)json_str);
        yyjson_mut_doc_free(mut_doc);
        return false;
    }
    
    size_t json_len = strlen(json_str);
    size_t written = fwrite(json_str, 1, json_len, out_file);
    fclose(out_file);
    
    // 清理资源 (Clean up resources)
    free((void*)json_str);
    yyjson_mut_doc_free(mut_doc);
    
    return written == json_len;
}

// 获取嵌套JSON键的值，如果根键或嵌套键不存在则返回根键
// Get nested JSON key value, return root key if root key or nested key doesn't exist
std::string JsonManager::GetNestedJsonValue(const std::string& json_path, const std::string& root_key, const std::string& nested_key) {
    // 确保JSON文件存在 (Ensure JSON file exists)
    if (!EnsureJsonFileExists(json_path)) {
        return root_key; // 文件不存在，返回根键 (File doesn't exist, return root key)
    }
    
    // 打开文件进行读取 (Open file for reading)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) {
        return root_key; // 无法打开文件，返回根键 (Cannot open file, return root key)
    }
    
    // 获取文件大小 (Get file size)
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 读取文件内容 (Read file content)
    char* json_data = (char*)malloc(file_size + 1);
    if (!json_data) {
        fclose(file);
        return root_key; // 内存分配失败，返回根键 (Memory allocation failed, return root key)
    }
    
    fread(json_data, 1, file_size, file);
    json_data[file_size] = '\0';
    fclose(file);
    
    // 解析JSON (Parse JSON)
    yyjson_doc* doc = yyjson_read(json_data, file_size, 0);
    free(json_data);
    if (!doc) {
        return root_key; // JSON解析失败，返回根键 (JSON parsing failed, return root key)
    }
    
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return root_key; // 根对象无效，返回根键 (Invalid root object, return root key)
    }
    
    // 查找根键对应的对象 (Find object for root key)
    yyjson_val* root_obj = yyjson_obj_get(root, root_key.c_str());
    if (!root_obj || !yyjson_is_obj(root_obj)) {
        yyjson_doc_free(doc);
        return root_key; // 根键不存在或不是对象，返回根键 (Root key doesn't exist or is not an object, return root key)
    }
    
    // 查找嵌套键对应的值 (Find value for nested key)
    yyjson_val* nested_val = yyjson_obj_get(root_obj, nested_key.c_str());
    if (!nested_val || !yyjson_is_str(nested_val)) {
        yyjson_doc_free(doc);
        return root_key; // 嵌套键不存在或不是字符串，返回根键 (Nested key doesn't exist or is not a string, return root key)
    }
    
    // 获取嵌套键的字符串值 (Get string value of nested key)
    const char* nested_str = yyjson_get_str(nested_val);
    std::string result = nested_str ? std::string(nested_str) : root_key;
    
    yyjson_doc_free(doc);
    return result;
}

} // namespace tj