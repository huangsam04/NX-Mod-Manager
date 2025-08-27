#include "mod_manager.hpp"
#include "lang_manager.hpp"
#include "miniz/miniz.h"
#include <switch.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <vector>
#include <set>

// 定义静态成员变量 (Define static member variable)
const std::string ModManager::target_directory_zip = "/atmosphere/";


ModManager::ModManager() {
    // 构造函数
}

ModManager::~ModManager() {
    // 析构函数
}





bool ModManager::extractMod(const std::string& zip_path,
                           ProgressCallback progress_callback,
                           ErrorCallback error_callback,
                           std::stop_token stop_token) {
    // 初始化ZIP读取器
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    
    if (!mz_zip_reader_init_file(&zip_archive, zip_path.c_str(), 0)) {
        if (error_callback) {
            error_callback(ZIP_OPEN_ERROR + zip_path);
        }
        return false;
    }
    
    // 获取文件数量
    int num_files = static_cast<int>(mz_zip_reader_get_num_files(&zip_archive));
    
    // 提取每个文件
    for (int i = 0; i < num_files; i++) {
        // 检查是否需要停止 (Check if stop is requested)
        if (stop_token.stop_requested()) {
            mz_zip_reader_end(&zip_archive);
            return false;
        }
        
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            mz_zip_reader_end(&zip_archive);
            if (error_callback) {
                error_callback(ZIP_READ_ERROR + zip_path);
            }
            return false;
        }
        
        // 更新进度 - 只显示文件名，不显示路径 (Update progress - show only filename, not path)
        if (progress_callback) {
            // 使用指针直接访问文件名，避免字符串拷贝 (Use pointer to access filename directly, avoid string copy)
            const char* filename = file_stat.m_filename;
            const char* last_slash = strrchr(filename, '/');
            const char* display_name = last_slash ? last_slash + 1 : filename;
            progress_callback(i + 1, num_files, display_name, false, 0.0f);
        }
        
        // 跳过目录条目
        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            continue;
        }
        
        // 构建完整的目标文件路径（目录已预创建）
        std::string target_file_path = normalizePath(target_directory_zip + file_stat.m_filename);
        
        // 再次检查是否需要停止 (Check again if stop is requested)
        if (stop_token.stop_requested()) {
            mz_zip_reader_end(&zip_archive);
            return false;
        }
        
        // 提取文件到内存
        size_t uncomp_size;
        void* file_data = mz_zip_reader_extract_file_to_heap(&zip_archive, 
                                                            file_stat.m_filename, 
                                                            &uncomp_size, 
                                                            0);
        if (!file_data) {
            mz_zip_reader_end(&zip_archive);
            if (error_callback) {
                error_callback(CANT_READ_ERROR + std::string(file_stat.m_filename));
            }
            return false;
        }
        
        // 写入文件
        FILE* output_file = fopen(target_file_path.c_str(), "wb");
        if (!output_file) {
            mz_free(file_data);
            mz_zip_reader_end(&zip_archive);
            if (error_callback) {
                error_callback(CANT_CREATE_ERROR + target_file_path);
            }
            return false;
        }
        
        size_t written = fwrite(file_data, 1, uncomp_size, output_file);
        fclose(output_file);
        mz_free(file_data);
        
        if (written != uncomp_size) {
            mz_zip_reader_end(&zip_archive);
            if (error_callback) {
                error_callback(CANT_WRITE_ERROR + target_file_path);
            }
            return false;
        }
    }
    
    mz_zip_reader_end(&zip_archive);
    return true;
}

// 带进度回调的MOD文件删除统计函数 (MOD file removal counting function with progress callback)
size_t ModManager::CountModFilesToRemoveWithProgress(const std::string& mod_source_path,
                                                    ProgressCallback progress_callback,
                                                    ErrorCallback error_callback,
                                                    size_t* current_counted,
                                                    std::stop_token stop_token) {
    size_t count = 0;
    
    DIR* source_dir = opendir(mod_source_path.c_str());
    if (!source_dir) {
        if (error_callback) {
            error_callback(CANT_OPEN_FILE + mod_source_path);
        }
        return 0;
    }
    
    struct dirent* entry;
    while ((entry = readdir(source_dir)) != nullptr) {
        // 检查是否需要停止 (Check if stop is requested)
        if (stop_token.stop_requested()) {
            closedir(source_dir);
            return count;
        }
        
        // 跳过当前目录和父目录 (Skip current and parent directory entries)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 构建完整路径 (Build full path)
        std::string full_path = mod_source_path + "/" + entry->d_name;
        
        // 使用d_type判断文件类型，避免stat()系统调用 (Use d_type to determine file type, avoiding stat() system call)
        if (entry->d_type == DT_DIR) {
            // 递归统计子目录 (Recursively count subdirectory)
            count += CountModFilesToRemoveWithProgress(full_path, progress_callback, error_callback, current_counted, stop_token);
        } else if (entry->d_type == DT_REG) {
            // 直接计算并缓存目标文件路径，避免后续重复计算 (Directly calculate and cache target file path to avoid repeated calculation)
            std::string target_file_path;
            
            // 查找exefs_patches或contents在路径中的位置 (Find position of exefs_patches or contents in path)
            size_t exefs_pos = full_path.find("/exefs_patches/");
            size_t contents_pos = full_path.find("/contents/");
            
            if (exefs_pos != std::string::npos) {
                // 使用指针直接构建目标路径，避免substr拷贝 (Use pointer to build target path directly, avoiding substr copy)
                const char* relative_start = full_path.c_str() + exefs_pos + 1; // +1 to skip the leading '/'
                target_file_path = target_directory_zip + relative_start;
                cached_target_files.push_back(target_file_path);
                count++;
                (*current_counted)++;
            } else if (contents_pos != std::string::npos) {
                // 使用指针直接构建目标路径，避免substr拷贝 (Use pointer to build target path directly, avoiding substr copy)
                const char* relative_start = full_path.c_str() + contents_pos + 1; // +1 to skip the leading '/'
                target_file_path = target_directory_zip + relative_start;
                cached_target_files.push_back(target_file_path);
                count++;
                (*current_counted)++;
            }
            // 如果路径中没有exefs_patches或contents，跳过此文件 (Skip if path doesn't contain exefs_patches or contents)
            
            // 每50个文件更新一次进度 (Update progress every 500 files)
            if ((*current_counted) % 50 == 0 && progress_callback) {
                progress_callback(0, *current_counted, CALCULATE_FILES, false, 0.0f);
            }
        }
    }
    
    closedir(source_dir);
    
    // 统计完成时更新最终进度 (Update final progress when counting is complete)
    if (progress_callback && count > 0) {
        progress_callback(0, *current_counted, CALCULATE_FILES, false, 0.0f);
    }
    
    return count;
}

// 基于缓存路径删除文件 (Remove files based on cached paths)
bool ModManager::RemoveModFilesFromCache(ProgressCallback progress_callback,
                                        ErrorCallback error_callback,
                                        std::stop_token stop_token) {
    bool overall_success = true;
    size_t processed_files = 0;
    size_t total_files = cached_target_files.size();
    
    std::string last_dir_path = ""; // 记录上一个文件的目录路径 (Record last file's directory path)
    
    for (const std::string& target_file_path : cached_target_files) {
        // 检查是否需要停止 (Check if stop is requested)
        if (stop_token.stop_requested()) {
            
            return false;
        }
        
        // 直接使用缓存的目标路径，无需重新计算 (Directly use cached target path, no need to recalculate)
        
        // 使用指针获取当前文件的目录路径，避免substr拷贝 (Use pointer to get current file's directory path, avoiding substr copy)
        size_t last_slash_pos = target_file_path.find_last_of('/');
        std::string current_dir;
        if (last_slash_pos != std::string::npos) {
            current_dir.assign(target_file_path.c_str(), last_slash_pos);
        }
        
        // 检查父目录是否改变，如果改变了说明前一个目录的源MOD文件已清空 (Check if parent directory changed)
        if (!last_dir_path.empty() && current_dir != last_dir_path) {
            // 检查并删除上一个目录（如果为空） (Check and delete previous directory if empty)
            std::string check_dir = last_dir_path;
            while (!check_dir.empty() && check_dir.length() > target_directory_zip.length()) {
                // 检查是否到达contents或exefs_patches目录，如果是则停止 (Check if reached contents or exefs_patches directory)
                const char* relative_start = check_dir.c_str() + target_directory_zip.length();
                if (strcmp(relative_start, "contents") == 0 || strcmp(relative_start, "exefs_patches") == 0) {
                    break; // 到达contents或exefs_patches目录，停止删除 (Reached contents or exefs_patches directory, stop deletion)
                }
                
                // 尝试删除目录，只有在目录不为空时才停止 (Try to delete directory, only stop if directory is not empty)
                if (remove(check_dir.c_str()) != 0) {
                    // 检查是否是因为目录不为空，如果是其他错误则继续 (Check if it's because directory is not empty)
                    if (errno == ENOTEMPTY || errno == EEXIST) {
                        break; // 目录不为空，停止向上检查 (Directory not empty, stop checking upward)
                    }
                }
                
                // 移动到父目录，使用指针避免substr拷贝 (Move to parent directory, use pointer to avoid substr copy)
                size_t parent_slash = check_dir.find_last_of('/');
                if (parent_slash == std::string::npos || parent_slash <= target_directory_zip.length()) {
                    break;
                }
                check_dir.assign(check_dir.c_str(), parent_slash);
            }
        }
        
        // 删除目标文件 (Delete target file)
        if (remove(target_file_path.c_str()) != 0) {
            if (errno != ENOENT) {
                // 只有非"文件不存在"的错误才认为是真正的失败 (Only non-ENOENT errors are considered real failures)
                overall_success = false;
                if (error_callback) {
                    error_callback(UNINSTALLED_ERROR + target_file_path + ", errno: " + std::to_string(errno));
                }
            }
            // ENOENT错误忽略，继续处理下一个文件 (Ignore ENOENT error, continue with next file)
        }
        
        // 更新当前目录路径 (Update current directory path)
        last_dir_path = current_dir;
        processed_files++;
        
        // 更新进度 (Update progress)
        if (progress_callback) {
            float progress_percentage = (float)processed_files / total_files * 100.0f;
            // 只显示文件名，不显示路径 (Show only filename, not path)
            const char* filename_ptr = target_file_path.c_str();
            const char* last_slash = strrchr(filename_ptr, '/');
            const char* display_name = last_slash ? last_slash + 1 : filename_ptr;
            progress_callback(processed_files, total_files, display_name, false, progress_percentage);
        }
    }
    
    // 处理完所有文件后，清理最后一个目录 (Clean up the last directory after processing all files)
    if (!last_dir_path.empty()) {
        std::string check_dir = last_dir_path;
        while (!check_dir.empty() && check_dir.length() > target_directory_zip.length()) {
            // 检查是否到达contents或exefs_patches目录，如果是则停止 (Check if reached contents or exefs_patches directory)
            const char* relative_start = check_dir.c_str() + target_directory_zip.length();
            if (strcmp(relative_start, "contents") == 0 || strcmp(relative_start, "exefs_patches") == 0) {
                break; // 到达contents或exefs_patches目录，停止删除 (Reached contents or exefs_patches directory, stop deletion)
            }
            
            // 尝试删除目录，只有在目录不为空时才停止 (Try to delete directory, only stop if directory is not empty)
            if (remove(check_dir.c_str()) != 0) {
                // 检查是否是因为目录不为空，如果是其他错误则继续 (Check if it's because directory is not empty)
                if (errno == ENOTEMPTY || errno == EEXIST) {
                    break; // 目录不为空，停止向上检查 (Directory not empty, stop checking upward)
                }
            }
            
            // 移动到父目录，使用指针避免substr拷贝 (Move to parent directory, use pointer to avoid substr copy)
            size_t parent_slash = check_dir.find_last_of('/');
            if (parent_slash == std::string::npos || parent_slash <= target_directory_zip.length()) {
                break;
            }
            check_dir.assign(check_dir.c_str(), parent_slash);
        }
    }
    
    return overall_success;
}

// 卸载文件夹类型的MOD（删除atmosphere目录中对应的文件）
bool ModManager::uninstallModFromFolder(const std::string& folder_path,
                                       ProgressCallback progress_callback,
                                       ErrorCallback error_callback,
                                       std::stop_token stop_token) {
    // 清空缓存，确保每次卸载都有干净的缓存状态 (Clear cache for clean state)
    cached_target_files.clear();
    
    // 打开MOD目录 (Open MOD directory)
    DIR* mod_dir = opendir(folder_path.c_str());
    if (!mod_dir) {
        if (error_callback) {
            error_callback(CANT_OPEN_FILE + folder_path);
        }
        return false;
    }
    
    std::vector<std::string> target_folders; // 目标文件夹列表 (Target folder list)
    
    struct dirent* entry;
    // 遍历MOD目录下的所有条目 (Iterate through all entries in MOD directory)
    while ((entry = readdir(mod_dir)) != nullptr) {
        // 跳过当前目录和父目录 (Skip current and parent directory entries)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 构建完整路径检查是否为目录 (Build full path to check if it's a directory)
        std::string full_path = folder_path + "/" + entry->d_name;
        DIR* test_dir = opendir(full_path.c_str());
        if (test_dir) {
            closedir(test_dir);
            
            // 检查文件夹名称是否符合要求 (Check if folder name meets requirements)
            std::string folder_name = entry->d_name;
            if (folder_name == "exefs_patches" || folder_name == "contents") {
                target_folders.push_back(folder_name);
            } else {
                // 文件夹名称不符合要求 (Folder name doesn't meet requirements)
                closedir(mod_dir);
                if (error_callback) {
                    error_callback(FILE_NONE + folder_name);
                }
                return false;
            }
        }
    }
    
    closedir(mod_dir);
    
    // 检查是否有有效的文件夹 (Check if there are valid folders)
    if (target_folders.empty()) {
        if (error_callback) {
            error_callback(FILE_NONE);
        }
        return false;
    }
    
    
    
    // 检查是否需要停止 (Check if stop is requested)
    if (stop_token.stop_requested()) {
        
        return false;
    }
    
    // 阶段1：统计所有文件夹需要删除的文件数量并缓存路径 (Phase 1: Count files to be removed in all folders and cache paths)
    size_t total_files = 0;
    size_t current_counted = 0;
    
    for (const std::string& folder : target_folders) {
        // 在统计过程中检查是否需要停止 (Check if stop is requested during counting)
        if (stop_token.stop_requested()) {
            
            return false;
        }
        
        std::string mod_source_path = folder_path + "/" + folder;
        total_files += CountModFilesToRemoveWithProgress(mod_source_path, progress_callback, error_callback, &current_counted, stop_token);
    }
    
    if (total_files == 0) {
        if (error_callback) {
            error_callback(FILE_NONE);
        }
        return false;
    }
    
    // 阶段2：基于缓存的路径进行删除 (Phase 2: Delete based on cached paths)
    bool overall_result = RemoveModFilesFromCache(progress_callback, error_callback, stop_token);
    
    if (!overall_result && error_callback) {
        error_callback(UNINSTALLED_ERROR);
    }
    
    // 清理缓存 (Clean up cache)
    cached_target_files.clear();
    cached_target_files.shrink_to_fit();
    
    return overall_result;
}

// 卸载ZIP类型的MOD (Uninstall ZIP type MOD)
bool ModManager::uninstallModFromZipDirect(const std::string& zip_path,
                                          ProgressCallback progress_callback,
                                          ErrorCallback error_callback,
                                          std::stop_token stop_token) {
    
    // 初始化ZIP读取器 (Initialize ZIP reader)
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    
    if (!mz_zip_reader_init_file(&zip_archive, zip_path.c_str(), 0)) {
        if (error_callback) {
            error_callback(ZIP_OPEN_ERROR + zip_path);
        }
        return false;
    }
    
    // 获取ZIP文件中的文件数量 (Get number of files in ZIP)
    int num_files = static_cast<int>(mz_zip_reader_get_num_files(&zip_archive));
    
    // 清空缓存并统计有效文件数量 (Clear cache and count valid files)
    cached_target_files.clear();
    int valid_file_count = 0;
    
    for (int i = 0; i < num_files; i++) {
        // 检查是否需要停止 (Check if stop is requested)
        if (stop_token.stop_requested()) {
            mz_zip_reader_end(&zip_archive);
            
            return false;
        }
        
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            mz_zip_reader_end(&zip_archive);
            if (error_callback) {
                error_callback(ZIP_READ_ERROR + zip_path);
            }
            return false;
        }
        
        std::string filename = file_stat.m_filename;
        
        // 跳过目录条目 (Skip directory entries)
        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            continue;
        }
        
        // 统计所有文件并构建目标路径 (Count all files and build target paths)
        valid_file_count++;
        
        // 构建目标文件路径并存储到缓存 (Build target file path and store to cache)
        std::string target_path = target_directory_zip + filename;
        cached_target_files.push_back(target_path);
    }
    
    mz_zip_reader_end(&zip_archive);
    
    // 对缓存的文件路径进行反向深度排序，确保删除时先删除最深的文件和目录
    // (Sort cached file paths by reverse depth to ensure deepest files and directories are deleted first)
    std::sort(cached_target_files.begin(), cached_target_files.end(), [](const std::string& a, const std::string& b) {
        // 按路径深度（斜杠数量）降序排序，深度相同时按字典序降序
        // (Sort by path depth (slash count) in descending order, lexicographically descending for same depth)
        size_t depth_a = std::count(a.begin(), a.end(), '/');
        size_t depth_b = std::count(b.begin(), b.end(), '/');
        if (depth_a != depth_b) {
            return depth_a > depth_b; // 深度大的排在前面 (Deeper paths come first)
        }
        return a > b; // 深度相同时按字典序降序 (Lexicographically descending for same depth)
    });
    
    // 输出统计信息用于调试 (Output statistics for debugging)
    if (progress_callback) {
        progress_callback(0, valid_file_count, CALCULATE_FILES, false, 0.0f);
    }
    
    // 调用RemoveModFilesFromCache函数完成实际的文件删除 (Call RemoveModFilesFromCache to perform actual file deletion)
    return RemoveModFilesFromCache(progress_callback, error_callback, stop_token);
}

// 判断MOD类型 (Determine MOD type)
bool ModManager::getModInstallType(const std::string& mod_path,
                                   int operation_type,
                                   ProgressCallback progress_callback,
                                   ErrorCallback error_callback,
                                   std::stop_token stop_token) {

    
    // 直接打开目录，打不开就报错 (Open directory directly, report error if failed)
    DIR* dir = opendir(mod_path.c_str());
    if (!dir) {
        if (error_callback) {
            error_callback(CANT_OPEN_FILE + mod_path);
        }
        return false;
    }
    
    // 读取mod路径下面的文件或者目录 (Read files or directories under mod path)
    bool has_contents = false;
    bool has_exefs_patches = false;
    bool has_zip_file = false;
    bool has_other_items = false;
    std::string zip_file_name = "";
    int total_items = 0;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        total_items++;
        
        // 直接使用d_type判断文件类型，避免stat()系统调用 (Use d_type directly to determine file type, avoiding stat() system call)
        if (entry->d_type == DT_DIR) {
            // 检查是否为contents或exefs_patches目录 (Check if it's contents or exefs_patches directory)
            if (strcmp(entry->d_name, "contents") == 0) {
                has_contents = true;
            } else if (strcmp(entry->d_name, "exefs_patches") == 0) {
                has_exefs_patches = true;
            } else {
                has_other_items = true;
            }
        } else if (entry->d_type == DT_REG) {
            // 检查是否为ZIP文件，使用指针避免substr拷贝 (Check if it's a ZIP file, use pointer to avoid substr copy)
            std::string filename = entry->d_name;
            size_t len = filename.length();
            if (len > 4 && strcmp(filename.c_str() + len - 4, ".zip") == 0) {
                has_zip_file = true;
                zip_file_name = filename;
            } else {
                has_other_items = true;
            }
        }
    }
    closedir(dir);
    
    // 检查是否需要停止 (Check if stop is requested)
    if (stop_token.stop_requested()) {
        
        return false;
    }
    
    // 如果mod路径下面只有contents或者exefs_patches或者这两个都有，视为文件类型，启动文件类型安装
    // (If mod path only has contents or exefs_patches or both, treat as file type, start file type installation)
    if ((has_contents || has_exefs_patches) && !has_other_items && !has_zip_file) {
        if (operation_type == 1) return installModFromFolder(mod_path, progress_callback, error_callback, stop_token);
        else if (operation_type == 0) return uninstallModFromFolder(mod_path, progress_callback, error_callback, stop_token);
    }
    
    // 否则如果mod路径下面只有文件且为ZIP文件，则视为ZIP类型启动ZIP安装方法
    // (Otherwise if mod path only has file and it's ZIP file, treat as ZIP type and start ZIP installation)
    if (has_zip_file && !has_contents && !has_exefs_patches && !has_other_items && total_items == 1) {
        std::string zip_path = mod_path + "/" + zip_file_name;
        if (operation_type == 1) return installModFromZipDirect(zip_path, progress_callback, error_callback, stop_token);
        else if (operation_type == 0) return uninstallModFromZipDirect(zip_path, progress_callback, error_callback, stop_token);
    }
    
    // 否则提示MOD结构不合法，结束安装 (Otherwise prompt MOD structure is invalid, end installation)
    if (error_callback) {
        error_callback(FILE_NONE);
    }
    return false;
}





bool ModManager::installModFromZipDirect(const std::string& zip_path,
                                        ProgressCallback progress_callback,
                                        ErrorCallback error_callback,
                                        std::stop_token stop_token) {
    
    
    
    progress_callback(0, 0, CALCULATE_FILES, false, 0.0f);

    // 初始化ZIP读取器检查内部结构
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    
    if (!mz_zip_reader_init_file(&zip_archive, zip_path.c_str(), 0)) {
        if (error_callback) {
            error_callback(ZIP_OPEN_ERROR + zip_path);
        }
        return false;
    }
    
    // 第一阶段：遍历ZIP文件收集所有目录
    std::vector<std::string> all_directories; // 直接使用vector收集目录列表
    std::set<std::string> first_level_dirs;
    int num_files = static_cast<int>(mz_zip_reader_get_num_files(&zip_archive));
    
    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            mz_zip_reader_end(&zip_archive);
            if (error_callback) {
                error_callback(ZIP_READ_ERROR + zip_path);
            }
            return false;
        }
        
        std::string filename = file_stat.m_filename;
        
        // 收集第一层目录信息
        size_t first_slash = filename.find('/');
        if (first_slash != std::string::npos) {
            std::string first_dir;
            first_dir.assign(filename.c_str(), first_slash); // 避免substr，直接使用assign减少临时对象创建
            first_level_dirs.insert(first_dir);
        }
        
        // 收集所有需要创建的目录路径（包括所有父目录层次）- 优化版本
        if (!mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            size_t last_slash = filename.find_last_of('/');
            if (last_slash != std::string::npos) {
                // 使用string_view避免字符串拷贝，优化路径解析性能
                // 直接使用指针和长度，避免substr的拷贝开销
                const char* relative_dir_data = filename.c_str();
                size_t relative_dir_length = last_slash;
                
                // 预分配基础路径，减少重复的字符串拼接开销
                std::string base_path;
                base_path.reserve(target_directory_zip.length() + last_slash + 1);
                base_path = target_directory_zip;
                
                // 一次性遍历路径，收集所有父目录层次（避免重复find操作）
                  for (size_t pos = 0; pos < relative_dir_length; ++pos) {
                      if (relative_dir_data[pos] == '/') {
                          // 构建父目录路径（从开始到当前斜杠位置）
                          std::string parent_dir;
                          parent_dir.reserve(base_path.length() + pos);
                          parent_dir = base_path;
                          parent_dir.append(relative_dir_data, pos);
                          all_directories.push_back(std::move(parent_dir));
                      }
                  }
                 
                 // 添加完整的目录路径（直接使用指针数据，避免额外拷贝）
                  std::string full_dir_path;
                  full_dir_path.reserve(base_path.length() + relative_dir_length);
                  full_dir_path = base_path;
                  full_dir_path.append(relative_dir_data, relative_dir_length);
                  all_directories.push_back(std::move(full_dir_path));
            }
        }
    }

    mz_zip_reader_end(&zip_archive);

    // 第二阶段：检查收集到的目录的合法性
    if (first_level_dirs.size() > 2 || first_level_dirs.empty()) {
        if (error_callback) {
            error_callback(FILE_NONE);
        }
        return false;
    }

    
    for (const std::string& dir_name : first_level_dirs) {
        if (dir_name != "contents" && dir_name != "exefs_patches") {
            if (error_callback) {
                error_callback(FILE_NONE + dir_name);
            }
            return false;
        }
    }
    
    // 使用批量创建目录函数优化性能 (Use batch directory creation function to optimize performance)
    if (!createDirectoriesBatch(all_directories, progress_callback, num_files, error_callback, stop_token)) {
        // 创建失败时及时清除目录列表，释放内存 (Clear directory list promptly on failure to free memory)
        all_directories.clear();
        all_directories.shrink_to_fit();
        return false;
    }
    
    // 目录创建成功后及时清除目录列表，释放内存 (Clear directory list promptly after successful creation to free memory)
    all_directories.clear();
    all_directories.shrink_to_fit();
    
    auto extract_progress = [&](int current, int total, const std::string& filename, bool is_copying_file, float file_progress_percentage) {
        if (progress_callback) {
            // 使用指针直接访问文件名，避免字符串拷贝 (Use pointer to access filename directly, avoid string copy)
            const char* filename_ptr = filename.c_str();
            const char* last_slash = strrchr(filename_ptr, '/');
            const char* display_name = last_slash ? last_slash + 1 : filename_ptr;
            // 显示当前个数/总个数格式 (Display current count/total count format)
            progress_callback(current, total, display_name, false, 0.0f);
        }
    };
    
    // 直接解压到atmosphere目录（目录已预创建）
    bool extract_success = extractMod(zip_path, extract_progress, error_callback, stop_token);
    
    if (extract_success && progress_callback) {
        progress_callback(num_files, num_files, "", false, 0.0f);
    }
    
    return extract_success;
}

bool ModManager::installModFromFolder(const std::string& folder_path,
                                      ProgressCallback progress_callback,
                                      ErrorCallback error_callback,
                                      std::stop_token stop_token) {

    


    progress_callback(0, 0, CALCULATE_FILES, false, 0.0f);

    // 检查MOD结构是否有效 (Check if MOD structure is valid)
    DIR* dir = opendir(folder_path.c_str());
    if (!dir) {
        if (error_callback) {
            error_callback(CANT_OPEN_FILE + folder_path);
        }
        return false;
    }
    
    bool has_contents = false;
    bool has_exefs_patches = false;
    bool has_other_items = false;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 直接使用d_type判断文件类型，避免stat()系统调用 (Use d_type directly to determine file type, avoiding stat() system call)
        if (entry->d_type == DT_DIR) {
            // 检查是否为contents或exefs_patches目录 (Check if it's contents or exefs_patches directory)
            if (strcmp(entry->d_name, "contents") == 0) {
                has_contents = true;
            } else if (strcmp(entry->d_name, "exefs_patches") == 0) {
                has_exefs_patches = true;
            } else {
                has_other_items = true;
            }
        } else if (entry->d_type == DT_REG) {
            // 发现文件，标记为有其他项目 (Found file, mark as having other items)
            has_other_items = true;
        }
    }
    closedir(dir);
    
    // 检查MOD结构合法性 (Check MOD structure validity)
    if ((!has_contents && !has_exefs_patches) || has_other_items) {
        if (error_callback) {
            error_callback(FILE_NONE);
        }
        return false;
    }
    
    // 统计文件数量并缓存路径 (Count files and cache paths)
    std::vector<std::pair<std::string, std::string>> cached_files; // 存储源路径和目标路径对 (Store source and target path pairs)
    std::vector<std::string> directories_to_create; // 存储需要创建的目录路径 (Store directory paths to be created)
    size_t total_files = 0;
    
    // 递归统计文件的函数 (Recursive function to count files)
    std::function<size_t(const std::string&, const std::string&)> count_files = 
        [&](const std::string& source_path, const std::string& target_path) -> size_t {
        size_t count = 0;
        DIR* dir = opendir(source_path.c_str());
        if (!dir) {
            // 无法打开源目录，输出错误信息用于诊断 (Cannot open source directory, output error for diagnosis)
            if (error_callback) {
                error_callback(CANT_OPEN_FILE + source_path + ", errno: " + std::to_string(errno));
            }
            return 0;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            // 优化：预分配字符串容量，减少内存重分配
            std::string source_file;
            source_file.reserve(source_path.length() + strlen(entry->d_name) + 2);
            source_file = source_path;
            source_file += '/';
            source_file += entry->d_name;
            
            std::string target_file;
            target_file.reserve(target_path.length() + strlen(entry->d_name) + 2);
            target_file = target_path;
            target_file += '/';
            target_file += entry->d_name;
            
            // 直接使用d_type判断文件类型，避免stat()系统调用 (Use d_type directly to determine file type, avoiding stat() system call)
            if (entry->d_type == DT_DIR) {
                // 收集目录路径，延迟创建 (Collect directory path for delayed creation)
                directories_to_create.push_back(target_file);
                // 递归处理子目录 (Recursively process subdirectory)
                count += count_files(source_file, target_file);
            } else if (entry->d_type == DT_REG) {
                // 缓存源文件路径和目标文件路径 (Cache source and target file paths)
                cached_files.push_back(std::make_pair(source_file, target_file)); // 存储源路径和目标路径对
                count++;
                
                // 每10个文件更新一次进度 (Update progress every 10 files)
                if (count % 50 == 0 && progress_callback) {
                    progress_callback(count, count, CALCULATE_FILES, false, 0.0f);
                }
            }
        }
        closedir(dir);
        return count;
    };
    
    // 统计exefs_patches目录 (Count exefs_patches directory) - 优化路径拼接
    if (has_exefs_patches) {
        std::string exefs_path;
        exefs_path.reserve(folder_path.length() + 16); // "/exefs_patches" = 15 chars + 1
        exefs_path = folder_path;
        exefs_path += "/exefs_patches";
        
        std::string target_exefs;
        target_exefs.reserve(target_directory_zip.length() + 15); // "exefs_patches" = 14 chars + 1
        target_exefs = target_directory_zip;
        target_exefs += "exefs_patches";
        
        total_files += count_files(exefs_path, target_exefs);
    }
    
    // 统计contents目录 (Count contents directory) - 优化路径拼接
    if (has_contents) {
        std::string contents_path;
        contents_path.reserve(folder_path.length() + 11); // "/contents" = 10 chars + 1
        contents_path = folder_path;
        contents_path += "/contents";
        
        std::string target_contents;
        target_contents.reserve(target_directory_zip.length() + 9); // "contents" = 8 chars + 1
        target_contents = target_directory_zip;
        target_contents += "contents";
        
        total_files += count_files(contents_path, target_contents);
    }
    
    if (total_files == 0) {
        if (error_callback) {
            error_callback(FILE_NONE);
        }
        return false;
    }
    
    // 批量创建所有目录 (Batch create all directories)
    if (!createDirectoriesBatch(directories_to_create, progress_callback, total_files, error_callback, stop_token)) {
        // 创建失败时及时清除目录列表，释放内存 (Clear directory list promptly on failure to free memory)
        directories_to_create.clear();
        directories_to_create.shrink_to_fit();
        return false;
    }
    
    // 目录创建成功后及时清除目录列表，释放内存 (Clear directory list promptly after successful creation to free memory)
    directories_to_create.clear();
    directories_to_create.shrink_to_fit();
    
    // 检查是否需要停止 (Check if stop is requested)
    if (stop_token.stop_requested()) {
        
        return false;
    }
    
    // 使用批量复制优化性能 (Use batch copy to optimize performance)
    if (!copyFilesBatch(cached_files, progress_callback, error_callback, stop_token)) {
        return false;
    }
    
    // 及时清理缓存的文件路径，释放内存 (Clean up cached file paths promptly to free memory)
    cached_files.clear();
    cached_files.shrink_to_fit();

    return true;
}



// 批量文件复制函数 - 优化版本，减少文件句柄开关和缓冲区分配
bool ModManager::copyFilesBatch(const std::vector<std::pair<std::string, std::string>>& file_pairs,
                               ProgressCallback progress_callback,
                               ErrorCallback error_callback,
                               std::stop_token stop_token) {
    if (file_pairs.empty()) {
        return true;
    }
    
    // 统一缓冲区策略 (Unified buffer strategy)
    const size_t LARGE_BUFFER_SIZE = 8 * 1024 * 1024;  // 8MB 统一缓冲区
    const size_t IO_BUFFER_SIZE = 1024 * 1024;         // 1MB I/O缓冲区，适合Switch平台
    
    // 分配缓冲区 (Allocate buffers)
    char* large_buffer = new char[LARGE_BUFFER_SIZE];
    
    int copied_files = 0;
    int total_files = file_pairs.size();
    bool overall_success = true;
    
    for (const auto& file_pair : file_pairs) {
        // 检查是否需要停止 (Check if stop is requested)
        if (stop_token.stop_requested()) {
            
            delete[] large_buffer;
            return false;
        }
        
        const std::string& source_path = file_pair.first;
        const std::string& dest_path = file_pair.second;
        
        // 打开源文件 (Open source file)
        FILE* source_file = fopen(source_path.c_str(), "rb");
        if (!source_file) {
            if (error_callback) {
                error_callback(CANT_OPEN_FILE + source_path);
            }
            overall_success = false;
            continue;
        }
        
        // 设置源文件缓冲区 (Set source file buffer)
        setvbuf(source_file, nullptr, _IOFBF, 1024 * 1024);
        
        // 获取文件大小 (Get file size)
        fseek(source_file, 0, SEEK_END);
        long file_size = ftell(source_file);
        fseek(source_file, 0, SEEK_SET);
        
        // 打开目标文件 (Open destination file)
        FILE* dest_file = fopen(dest_path.c_str(), "wb");
        if (!dest_file) {
            fclose(source_file);
            if (error_callback) {
                error_callback(CANT_CREATE_DIR + dest_path + ", errno: " + std::to_string(errno));
            }
            overall_success = false;
            continue;
        }
        
        // 设置目标文件缓冲区 (Set destination file buffer)
        setvbuf(dest_file, nullptr, _IOFBF, 1024 * 1024);
        
        bool file_success = true;
        
        // 提前提取文件名，避免重复计算 (Extract filename early to avoid repeated calculation)
        size_t last_slash = source_path.find_last_of('/');
        const char* filename_ptr = (last_slash != std::string::npos) ? 
            source_path.c_str() + last_slash + 1 : source_path.c_str();
        
        // 统一使用8MB缓冲区分块复制所有文件 (Use 8MB buffer for chunked copy of all files)
        size_t total_read = 0;
        int last_progress = -1;  // 移除静态变量，改为局部变量 (Remove static variable, use local variable)
        
        while (total_read < (size_t)file_size && file_success) {
            // 在文件复制过程中检查是否需要停止 (Check if stop is requested during file copy)
            if (stop_token.stop_requested()) {
                fclose(source_file);
                fclose(dest_file);
                delete[] large_buffer;
                
                return false;
            }
            
            size_t to_read = std::min(LARGE_BUFFER_SIZE, (size_t)file_size - total_read);
            size_t bytes_read = fread(large_buffer, 1, to_read, source_file);
            if (bytes_read > 0) {
                if (fwrite(large_buffer, 1, bytes_read, dest_file) != bytes_read) {
                    file_success = false;
                    break;
                }
                total_read += bytes_read;
                
                // 更新文件级进度 - 仅对大于8MB的文件显示 (Update file-level progress - only show for files larger than 8MB)
                if (progress_callback && file_size > 8 * 1024 * 1024) {
                    int progress_percent = (int)((total_read * 100) / file_size);
                    if (progress_percent - last_progress >= 5 || progress_percent >= 100) {
                        progress_callback(copied_files, total_files, filename_ptr, true, (float)progress_percent);
                        last_progress = progress_percent;
                    }
                }
            }
            if (bytes_read < to_read) break;
        }
        
        // 统一的错误处理 (Unified error handling)
        if (!file_success && error_callback) {
            if (ferror(source_file)) {
                error_callback(CANT_READ_ERROR + source_path);
            } else if (ferror(dest_file)) {
                error_callback(CANT_WRITE_ERROR + dest_path);
            } else {
                error_callback(CANT_WRITE_ERROR + dest_path);
            }
        }
        
        // 关闭文件句柄 (Close file handles)
        fclose(source_file);
        fclose(dest_file);
        
        if (!file_success) {
            overall_success = false;
        } else {
            copied_files++;
            
            // 更新总体进度 (Update overall progress)
            if (progress_callback) {
                progress_callback(copied_files, total_files, filename_ptr, false, 0.0f);
            }
        }
    }
    
    // 清理缓冲区 (Clean up buffers)
    delete[] large_buffer;
    
    return overall_success;
}

// 批量创建目录函数
bool ModManager::createDirectoriesBatch(std::vector<std::string>& directories, 
                                       ProgressCallback progress_callback, 
                                       size_t total_files,
                                       ErrorCallback error_callback,
                                       std::stop_token stop_token) {
    if (directories.empty()) {
        return true;
    }
    
    // 按路径长度排序，确保父目录先创建 (Sort by path length to ensure parent directories are created first)
    std::sort(directories.begin(), directories.end());
    
    // 去重，避免重复创建 (Remove duplicates to avoid redundant creation)
    directories.erase(std::unique(directories.begin(), directories.end()), directories.end());
    
    // 批量创建目录 (Batch create directories)
    for (const std::string& dir_path : directories) {
        // 检查是否需要停止 (Check if stop is requested)
        if (stop_token.stop_requested()) {
            
            return false;
        }
        
        if (mkdir(dir_path.c_str(), 0755) != 0 && errno != EEXIST) {
            if (error_callback) {
                error_callback(CANT_CREATE_DIR + dir_path + ", errno: " + std::to_string(errno));
            }
            return false;
        }
    }
    
    // 更新进度：目录创建完成 (Update progress: directory creation completed)
    if (progress_callback) {
        progress_callback(0, total_files, CALCULATE_FILES, false, 0.0f);
    }
    
    return true;
}

std::string ModManager::normalizePath(const std::string& path) {
    std::string normalized = path;
    
    // 将反斜杠转换为正斜杠
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    
    // 移除重复的斜杠
    size_t pos = 0;
    while ((pos = normalized.find("//", pos)) != std::string::npos) {
        normalized.erase(pos, 1);
    }
    
    // 移除末尾的斜杠（除非是根目录）
    if (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    return normalized;
}