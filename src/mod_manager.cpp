#include "mod_manager.hpp"
#include "lang_manager.hpp"
#include "json_manager.hpp"  // 添加JSON管理器头文件
#include "miniz/miniz.h"
#include "utils/logger.hpp"  // 添加日志头文件
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
#include <cstdlib>  // for free
#include <malloc.h>  // for memalign

// 定义静态成员变量 (Define static member variable)
const std::string ModManager::target_directory_zip = "/atmosphere/";


ModManager::ModManager() {
    // 构造函数
}

ModManager::~ModManager() {
    // 析构函数
}

// 顺序写入，比extractMod2函数快几秒，真奇怪
// extractMod函数：简化版本，不使用小文件聚合，直接按顺序写入SD卡 (extractMod2 function: simplified version, no small file aggregation, direct sequential write to SD card)
bool ModManager::extractMod(const std::string& zip_path,
                            int& files_total,
                            ProgressCallback progress_callback,
                            ErrorCallback error_callback,
                            std::stop_token stop_token,
                            void* zip_archive_ptr) {
    
    // 直接使用传入的已初始化ZIP读取器
    mz_zip_archive* zip_archive = static_cast<mz_zip_archive*>(zip_archive_ptr);
    
    // 获取文件数量 (Get file count)
    int num_files = static_cast<int>(mz_zip_reader_get_num_files(zip_archive));
    int processed_files = 0;
    
    // SD卡块对齐优化策略 (SD Card Block Alignment Optimization Strategy)
    const size_t SD_BLOCK_SIZE = 64 * 1024; // 64KB SD卡块大小
    const size_t ALIGNED_BUFFER_SIZE = 32 * 1024 * 1024; // 32MB 对齐缓冲区
    
    // 分配块对齐缓冲区 (Allocate block-aligned buffers)
    char* aligned_buffer = (char*)memalign(SD_BLOCK_SIZE, ALIGNED_BUFFER_SIZE + SD_BLOCK_SIZE);
    
    if (!aligned_buffer) {
        if (error_callback) {
            error_callback("内存分配失败 (Memory allocation failed)");
        }
        return false;
    }
    
    bool overall_success = true;
    
    // 提取每个文件，直接按顺序写入 (Extract each file, write directly in sequence)
    for (int i = 0; i < num_files; i++) {
        // 检查是否需要停止 (Check if stop is requested)
        if (stop_token.stop_requested()) {
            free(aligned_buffer);
            return false;
        }
        
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip_archive, i, &file_stat)) {
            free(aligned_buffer);
            if (error_callback) {
                error_callback(ZIP_READ_ERROR + zip_path);
            }
            return false;
        }
        
        // 跳过目录条目 (Skip directory entries)
        if (mz_zip_reader_is_file_a_directory(zip_archive, i)) {
            continue;
        }
        
        // 构建完整的目标文件路径 (Build complete target file path)
        std::string target_file_path = target_directory_zip + file_stat.m_filename;
        
        // 再次检查是否需要停止 (Check again if stop is requested)
        if (stop_token.stop_requested()) {
            free(aligned_buffer);
            return false;
        }
        
        // 创建流式解压迭代器 (Create streaming extraction iterator)
        mz_zip_reader_extract_iter_state* iter_state = mz_zip_reader_extract_iter_new(zip_archive, i, 0);
        if (!iter_state) {
            free(aligned_buffer);
            if (error_callback) {
                error_callback(CANT_READ_ERROR + std::string(file_stat.m_filename));
            }
            return false;
        }
        
        size_t uncomp_size = file_stat.m_uncomp_size;
        long file_size = static_cast<long>(uncomp_size);
        
        // 打开目标文件 (Open destination file)
        FILE* dest_file = fopen(target_file_path.c_str(), "wb");
        if (!dest_file) {
            mz_zip_reader_extract_iter_free(iter_state);
            free(aligned_buffer);
            if (error_callback) {
                error_callback(CANT_CREATE_ERROR + target_file_path + ", errno: " + std::to_string(errno));
            }
            return false;
        }
        
        // SD卡块对齐写入缓冲区 (SD Card block-aligned write buffer)
        setvbuf(dest_file, nullptr, _IOFBF, ALIGNED_BUFFER_SIZE);
        
        bool file_success = true;
        
        // 提前提取文件名，避免重复计算 (Extract filename early to avoid repeated calculation)
        const char* filename = file_stat.m_filename;
        const char* last_slash = strrchr(filename, '/');
        const char* filename_ptr = last_slash ? last_slash + 1 : filename;
        
        // 直接流式解压写入文件 (Direct streaming extraction and write to file)
        size_t total_written = 0;
        int last_progress = -1;
        
        while (total_written < (size_t)file_size && file_success) {
            // 在文件解压过程中检查是否需要停止 (Check if stop is requested during file extraction)
            if (stop_token.stop_requested()) {
                mz_zip_reader_extract_iter_free(iter_state);
                fclose(dest_file);
                free(aligned_buffer);
                return false;
            }
            
            // 计算块对齐的读取大小 (Calculate block-aligned read size)
            size_t remaining = (size_t)file_size - total_written;
            size_t to_read = std::min(ALIGNED_BUFFER_SIZE, remaining);
            
            size_t bytes_read = mz_zip_reader_extract_iter_read(iter_state, aligned_buffer, to_read);
            if (bytes_read > 0) {
                // 写入实际读取的字节数，保持文件原始大小 (Write actual bytes read, maintain original file size)
                if (fwrite(aligned_buffer, 1, bytes_read, dest_file) != bytes_read) {
                    file_success = false;
                    break;
                }
                total_written += bytes_read;
                
                // 更新文件级进度 (Update file-level progress)
                if (progress_callback && file_size > 8 * 1024 * 1024) {
                    int progress_percent = (int)((total_written * 100) / file_size);
                    // 实时更新进度 (Real-time progress update)
                    progress_callback(processed_files, files_total, filename_ptr, true, (float)progress_percent);
                    last_progress = progress_percent;
                }
            }
            if (bytes_read < to_read) break;
        }
        
        // 统一的错误处理 (Unified error handling)
        if (!file_success && error_callback) {
            if (ferror(dest_file)) {
                error_callback(CANT_WRITE_ERROR + target_file_path);
            } else {
                error_callback(CANT_WRITE_ERROR + target_file_path);
            }
        }
        

        
        // 关闭文件句柄和清理资源 (Close file handles and cleanup resources)
        mz_zip_reader_extract_iter_free(iter_state);
        fclose(dest_file);
        
        if (!file_success) {
            overall_success = false;
        } else {
            processed_files++;
            
            // 更新总体进度 (Update overall progress)
            if (progress_callback) {
                progress_callback(processed_files, files_total, filename_ptr, false, 0.0f);
            }
        }
    }
    
    // 清理块对齐缓冲区 (Clean up block-aligned buffers)
    free(aligned_buffer);
    
    return overall_success;
}

// // 带聚合的速度不如顺序的，备用不删了
// bool ModManager::extractMod2(const std::string& zip_path,
//                            ProgressCallback progress_callback,
//                            ErrorCallback error_callback,
//                            std::stop_token stop_token) {
    
//     // 初始化ZIP读取器 (Initialize ZIP reader)
//     mz_zip_archive zip_archive;
//     memset(&zip_archive, 0, sizeof(zip_archive));
    
//     if (!mz_zip_reader_init_file(&zip_archive, zip_path.c_str(), 0)) {
//         if (error_callback) {
//             error_callback(ZIP_OPEN_ERROR + zip_path);
//         }
//         return false;
//     }
    
//     // 获取文件数量 (Get file count)
//     int num_files = static_cast<int>(mz_zip_reader_get_num_files(&zip_archive));
//     int processed_files = 0;
    
//     // SD卡块对齐优化策略 (SD Card Block Alignment Optimization Strategy)
//     const size_t SD_BLOCK_SIZE = 64 * 1024; // 64KB SD卡块大小
//     const size_t ALIGNED_BUFFER_SIZE = 32 * 1024 * 1024; // 32MB 对齐缓冲区 (512个块)
//     const size_t SMALL_FILE_THRESHOLD = 8 * 1024 * 1024;   // 8MB 小文件阈值
//     const size_t BATCH_MEMORY_LIMIT = 32 * 1024 * 1024; // 32MB 批量内存限制
    
//     // 分配块对齐缓冲区 (Allocate block-aligned buffers)
//     // Switch平台使用memalign进行内存对齐到SD卡块边界
//     char* aligned_buffer = (char*)memalign(SD_BLOCK_SIZE, ALIGNED_BUFFER_SIZE + SD_BLOCK_SIZE);
    
//     if (!aligned_buffer) {
//         mz_zip_reader_end(&zip_archive);
//         if (error_callback) {
//             error_callback("内存分配失败 (Memory allocation failed)");
//         }
//         return false;
//     }
    
//     /* SD卡优化策略总结 (SD Card Optimization Strategy Summary):
//      * 1. 块对齐写入：所有写入操作对齐到64KB块边界，避免跨块写入
//      * 2. 顺序I/O优化：按文件路径排序，减少随机访问模式
//      * 3. 批量连续写入：小文件合并写入，减少写入次数
//      * 4. 减少fsync频率：批量刷新降低写入放大
//      * 5. 内存对齐：缓冲区对齐到块边界，提高传输效率
//      * 6. 块级缓存：使用64KB整数倍缓冲区，匹配SD卡特性
//      * 7. 写入地址对齐：确保文件写入位置对齐到块边界
//      * 8. 智能填充：不足块大小的数据进行零填充对齐
//      */
    
//     bool overall_success = true;
    
//     // 优化：单次遍历处理，小文件累积到内存池，大文件立即处理 (Optimization: single-pass processing, accumulate small files in memory pool, process large files immediately)
//     std::vector<std::pair<std::string, std::vector<char>>> cached_files;
//     size_t total_cached_size = 0;
    
//     // 批量写入缓存小文件的辅助函数 (Helper function to batch write cached small files)
//     auto flush_cached_files = [&]() -> bool {
//         for (const auto& cached : cached_files) {
//             FILE* dest_file = fopen(cached.first.c_str(), "wb");
//             if (dest_file) {
//                 // SD卡块对齐写入优化 (SD Card block-aligned write optimization)
//                 setvbuf(dest_file, nullptr, _IOFBF, ALIGNED_BUFFER_SIZE);
                
//                 size_t written = fwrite(cached.second.data(), 1, cached.second.size(), dest_file);
//                 fclose(dest_file);
                
//                 if (written == cached.second.size()) {
//                     processed_files++;
//                     if (progress_callback) {
//                         size_t last_slash = cached.first.find_last_of('/');
//                         const char* filename_ptr = (last_slash != std::string::npos) ? 
//                             cached.first.c_str() + last_slash + 1 : cached.first.c_str();
//                         progress_callback(processed_files, num_files, filename_ptr, false, 0.0f);
//                     }
//                 } else {
//                     if (error_callback) {
//                         error_callback(CANT_WRITE_ERROR + cached.first);
//                     }
//                     return false;
//                 }
//             } else {
//                 if (error_callback) {
//                     error_callback(CANT_CREATE_ERROR + cached.first);
//                 }
//                 return false;
//             }
//         }
//         cached_files.clear();
//         total_cached_size = 0;
//         return true;
//     };
    
//     // 提取每个文件 (Extract each file)
//     for (int i = 0; i < num_files; i++) {
//         // 检查是否需要停止 (Check if stop is requested)
//         if (stop_token.stop_requested()) {
//             mz_zip_reader_end(&zip_archive);
//             return false;
//         }
        
//         mz_zip_archive_file_stat file_stat;
//         if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
//             mz_zip_reader_end(&zip_archive);
//             if (error_callback) {
//                 error_callback(ZIP_READ_ERROR + zip_path);
//             }
//             return false;
//         }
        
//         // 跳过目录条目 (Skip directory entries)
//         if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
//             continue;
//         }
        
//         // 构建完整的目标文件路径（目录已预创建）(Build complete target file path - directories are pre-created)
//         std::string target_file_path = target_directory_zip + file_stat.m_filename;
        
//         // 再次检查是否需要停止 (Check again if stop is requested)
//         if (stop_token.stop_requested()) {
//             mz_zip_reader_end(&zip_archive);
//             return false;
//         }
        
//         // 获取文件大小用于判断处理策略 (Get file size for processing strategy decision)
//         size_t uncomp_size = file_stat.m_uncomp_size;
        
//         // 根据文件大小选择处理策略 (Choose processing strategy based on file size)
//         if (uncomp_size <= SMALL_FILE_THRESHOLD) {
//             // 处理小文件：累积到内存池 (Process small files: accumulate in memory pool)
            
//             // 检查内存限制，如果超出则先写入已缓存的文件 (Check memory limit, flush cached files if exceeded)
//             if (total_cached_size + uncomp_size > BATCH_MEMORY_LIMIT) {
//                 if (!flush_cached_files()) {
//                     free(aligned_buffer);
//                     mz_zip_reader_end(&zip_archive);
//                     return false;
//                 }
//             }
            
//             // 创建流式解压迭代器 (Create streaming extraction iterator)
//             mz_zip_reader_extract_iter_state* iter_state = mz_zip_reader_extract_iter_new(&zip_archive, i, 0);
//             if (!iter_state) {
//                 free(aligned_buffer);
//                 mz_zip_reader_end(&zip_archive);
//                 if (error_callback) {
//                     error_callback(CANT_READ_ERROR + std::string(file_stat.m_filename));
//                 }
//                 return false;
//             }
            
//             // SD卡块对齐读取优化 (SD Card block-aligned read optimization)
//             size_t file_size = uncomp_size;
//             size_t aligned_size = ((file_size + SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE) * SD_BLOCK_SIZE;
//             std::vector<char> file_data;
//             file_data.reserve(aligned_size);
//             file_data.resize(aligned_size, 0);
            
//             // 流式读取小文件数据 (Stream read small file data)
//             const size_t SMALL_STREAM_BUFFER_SIZE = 64 * 1024; // 64KB 缓冲区 (64KB buffer)
//             size_t total_read = 0;
//             size_t bytes_read;
//             bool read_success = true;
            
//             while ((bytes_read = mz_zip_reader_extract_iter_read(iter_state, aligned_buffer, SMALL_STREAM_BUFFER_SIZE)) > 0) {
//                 if (stop_token.stop_requested()) {
//                     read_success = false;
//                     break;
//                 }
                
//                 if (total_read + bytes_read <= file_size) {
//                     memcpy(file_data.data() + total_read, aligned_buffer, bytes_read);
//                     total_read += bytes_read;
//                 } else {
//                     read_success = false;
//                     break;
//                 }
//             }
            
//             mz_zip_reader_extract_iter_free(iter_state);
            
//             if (!read_success || total_read != file_size) {
//                 free(aligned_buffer);
//                 mz_zip_reader_end(&zip_archive);
//                 if (error_callback) {
//                     error_callback(CANT_READ_ERROR + std::string(file_stat.m_filename));
//                 }
//                 return false;
//             }
            
//             // 将文件数据添加到缓存 (Add file data to cache)
//             cached_files.push_back({target_file_path, std::move(file_data)});
//             total_cached_size += aligned_size;
            
//             // 更新进度 - 显示正在聚合小文件 (Update progress - show aggregating small files)
//             if (progress_callback) {
//                 progress_callback(processed_files, num_files, Aggregating_Files, false, 0.0f);
//             }
            
//         } else {
//             // 处理大文件：先写入所有缓存的小文件，然后立即处理大文件 (Process large files: flush all cached small files first, then process large file immediately)
//             if (!cached_files.empty()) {
//                 if (!flush_cached_files()) {
//                     free(aligned_buffer);
//                     mz_zip_reader_end(&zip_archive);
//                     return false;
//                 }
//             }
            
//             // 创建流式解压迭代器 (Create streaming extraction iterator)
//             mz_zip_reader_extract_iter_state* iter_state = mz_zip_reader_extract_iter_new(&zip_archive, i, 0);
//             if (!iter_state) {
//                 free(aligned_buffer);
//                 mz_zip_reader_end(&zip_archive);
//                 if (error_callback) {
//                     error_callback(CANT_READ_ERROR + std::string(file_stat.m_filename));
//                 }
//                 return false;
//             }
            
//             long file_size = static_cast<long>(uncomp_size);
            
//             // 打开目标文件 (Open destination file)
//             FILE* dest_file = fopen(target_file_path.c_str(), "wb");
//             if (!dest_file) {
//                 mz_zip_reader_extract_iter_free(iter_state);
//                 free(aligned_buffer);
//                 mz_zip_reader_end(&zip_archive);
//                 if (error_callback) {
//                     error_callback(CANT_CREATE_ERROR + target_file_path + ", errno: " + std::to_string(errno));
//                 }
//                 return false;
//             }
            
//             // SD卡块对齐写入缓冲区 (SD Card block-aligned write buffer)
//             setvbuf(dest_file, nullptr, _IOFBF, ALIGNED_BUFFER_SIZE);
            
//             bool file_success = true;
            
//             // 提前提取文件名，避免重复计算 (Extract filename early to avoid repeated calculation)
//             const char* filename = file_stat.m_filename;
//             const char* last_slash = strrchr(filename, '/');
//             const char* filename_ptr = last_slash ? last_slash + 1 : filename;
            
//             // SD卡块对齐分块解压大文件 (SD Card block-aligned chunked extraction of large files)
//             size_t total_written = 0;
//             int last_progress = -1;
            
//             while (total_written < (size_t)file_size && file_success) {
//                 // 在文件解压过程中检查是否需要停止 (Check if stop is requested during file extraction)
//                 if (stop_token.stop_requested()) {
//                     mz_zip_reader_extract_iter_free(iter_state);
//                     fclose(dest_file);
//                     free(aligned_buffer);
//                     mz_zip_reader_end(&zip_archive);
//                     return false;
//                 }
                
//                 // 计算块对齐的读取大小 (Calculate block-aligned read size)
//                 size_t remaining = (size_t)file_size - total_written;
//                 size_t to_read = std::min(ALIGNED_BUFFER_SIZE, remaining);
                
//                 size_t bytes_read = mz_zip_reader_extract_iter_read(iter_state, aligned_buffer, to_read);
//                 if (bytes_read > 0) {
//                     // 如果是最后一块且不足块大小，对齐到块边界 (If last chunk and less than block size, align to block boundary)
//                     size_t write_size = bytes_read;
//                     if (remaining < ALIGNED_BUFFER_SIZE && (bytes_read % SD_BLOCK_SIZE) != 0) {
//                         size_t aligned_write = ((bytes_read + SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE) * SD_BLOCK_SIZE;
//                         // 清零填充区域 (Zero-fill padding area)
//                         memset(aligned_buffer + bytes_read, 0, aligned_write - bytes_read);
//                         write_size = aligned_write;
//                     }
                    
//                     if (fwrite(aligned_buffer, 1, write_size, dest_file) != write_size) {
//                         file_success = false;
//                         break;
//                     }
//                     total_written += bytes_read;
                    
//                     // 更新文件级进度 - 仅对大文件显示 (Update file-level progress - only show for large files)
//                     if (progress_callback && file_size > 8 * 1024 * 1024) {
//                         int progress_percent = (int)((total_written * 100) / file_size);
//                         // 实时更新进度，不限制更新频率 (Real-time progress update without frequency limitation)
//                         // 使用 processed_files + 1 来表示当前正在处理的文件索引 (Use processed_files + 1 to represent current file index being processed)
//                         progress_callback(processed_files, num_files, filename_ptr, true, (float)progress_percent);
//                         last_progress = progress_percent;
//                     }
//                 }
//                 if (bytes_read < to_read) break;
//             }
            
//             // 统一的错误处理 (Unified error handling)
//             if (!file_success && error_callback) {
//                 if (ferror(dest_file)) {
//                     error_callback(CANT_WRITE_ERROR + target_file_path);
//                 } else {
//                     error_callback(CANT_WRITE_ERROR + target_file_path);
//                 }
//             }
            
            
//             // 关闭文件句柄和清理资源 (Close file handles and cleanup resources)
//             mz_zip_reader_extract_iter_free(iter_state);
//             fclose(dest_file);
            
//             if (!file_success) {
//                 overall_success = false;
//             } else {
//                 processed_files++;
                
//                 // 更新总体进度 (Update overall progress)
//                 if (progress_callback) {
//                     progress_callback(processed_files, num_files, filename_ptr, false, 0.0f);
//                 }
//             }
//         }
//     }
    
//     // 处理剩余的小文件缓存 (Process remaining cached small files)
//     if (!flush_cached_files()) {
//         overall_success = false;
//     }
    
//     // 清理块对齐缓冲区 (Clean up block-aligned buffers)
//     free(aligned_buffer); // 使用free释放memalign分配的内存
    
//     mz_zip_reader_end(&zip_archive);
//     return overall_success;
// }










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
            
            // 每10个文件更新一次进度 (Update progress every 500 files)
            if ((*current_counted) % 10 == 0 && progress_callback) {
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
    
    // 优化的目录聚集排序算法：按目录路径分组，确保完全聚集
    // (Optimized directory clustering sort algorithm: group by directory path for complete clustering)
    std::sort(cached_target_files.begin(), cached_target_files.end(), [](const std::string& a, const std::string& b) {
        // 提取目录路径的辅助函数
        auto get_directory = [](const std::string& path) {
            size_t last_slash = path.find_last_of('/');
            return (last_slash != std::string::npos) ? path.substr(0, last_slash) : "";
        };
        
        // 计算路径深度的辅助函数
        auto count_depth = [](const std::string& path) {
            return std::count(path.begin(), path.end(), '/');
        };
        
        std::string dir_a = get_directory(a);
        std::string dir_b = get_directory(b);
        
        // 1. 首先按目录路径字典序排序（确保同目录文件聚集）
        if (dir_a != dir_b) {
            return dir_a < dir_b;
        }
        
        // 2. 同目录内，优先按深度降序排序（深层文件先删除）
        int depth_a = count_depth(a);
        int depth_b = count_depth(b);
        if (depth_a != depth_b) {
            return depth_a > depth_b;
        }
        
        // 3. 深度相同时，按文件名字典序排序
        return a < b;
    });
    
    // // 输出cached_target_files到日志文件进行调试 (Output cached_target_files to log for debugging)
    // utils::Logger::Info("=== ZIP卸载文件列表开始 ===");
    // utils::Logger::Info("总文件数量: " + std::to_string(cached_target_files.size()));
    // for (size_t i = 0; i < cached_target_files.size(); ++i) {
    //     utils::Logger::Info("[" + std::to_string(i + 1) + "/" + std::to_string(cached_target_files.size()) + "] " + cached_target_files[i]);
    // }
    // utils::Logger::Info("=== ZIP卸载文件列表结束 ===");
    // return true;

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
    
    
    if (progress_callback) {
        progress_callback(0, 0, CALCULATE_FILES, false, 0.0f);
    }

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
    int files_total = 0;
    
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
            
            // 检查目标文件是否已存在，如果存在则立即报错并停止
            std::string target_file_path;
            target_file_path.reserve(target_directory_zip.length() + filename.length());
            target_file_path = target_directory_zip;
            target_file_path += filename;
            
            // 使用access()检查文件是否存在（F_OK表示检查文件存在性）
            if (access(target_file_path.c_str(), F_OK) == 0) {
                // 文件已存在，关闭ZIP读取器并报错
                mz_zip_reader_end(&zip_archive);
                if (error_callback) {
                    error_callback("文件冲突：目标文件已存在 - " + target_file_path);
                }
                return false;
            }
            
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
            
            files_total++;

            // 每500个文件更新一次进度
            if (progress_callback && files_total % 500 == 0 ) {
                progress_callback(0, files_total, CALCULATE_FILES, false, 0.0f);
            }

        }

    }

    

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
            progress_callback(current, total, display_name, is_copying_file, file_progress_percentage);
        }
    };
    
    // 直接解压到atmosphere目录（目录已预创建）
    bool extract_success = extractMod(zip_path, files_total, extract_progress, error_callback, stop_token, &zip_archive);
    //完成解压后关闭ZIP读取器
    mz_zip_reader_end(&zip_archive);
    
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
    
    // FileInfo结构体现在定义在头文件中 (FileInfo structure is now defined in header file)
    
    // 统计文件数量并缓存路径 (Count files and cache paths)
    std::vector<FileInfo> cached_files; // 存储文件信息 (Store file information)
    std::vector<std::string> directories_to_create; // 存储需要创建的目录路径 (Store directory paths to be created)
    size_t total_files = 0;
    size_t global_file_count = 0; // 全局累计计数器 (Global cumulative counter)
    
    // 递归统计文件的函数 (Recursive function to count files)
    std::function<size_t(const std::string&, const std::string&)> count_files = 
        [&](const std::string& source_path, const std::string& target_path) -> size_t {
        size_t local_count = 0; // 当前目录的文件数 (File count for current directory)
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
                local_count += count_files(source_file, target_file);
            } else if (entry->d_type == DT_REG) {
                // 获取文件大小并缓存文件信息 (Get file size and cache file info)
                struct stat file_stat;
                if (stat(source_file.c_str(), &file_stat) == 0) {
                    // 存储文件信息到结构体 (Store file info to structure)
                    cached_files.push_back({source_file, target_file, static_cast<size_t>(file_stat.st_size)});
                    local_count++;
                    global_file_count++; // 增加全局计数器 (Increment global counter)
                } else {
                    // 处理获取文件信息失败的情况 (Handle file info retrieval failure)
                    if (error_callback) {
                        error_callback("Cannot get file info: " + source_file + ", errno: " + std::to_string(errno));
                    }
                    return false;
                }
                
                // 每10个文件更新一次进度，使用全局计数器 (Update progress every 10 files using global counter)
                if (global_file_count % 10 == 0 && progress_callback) {
                    progress_callback(0, global_file_count, CALCULATE_FILES, false, 0.0f);
                }
            }
        }
        closedir(dir);
        
        // 不再显示当前目录的文件数，避免进度跳跃 (No longer show current directory file count to avoid progress jumping)
        // 进度更新已在文件处理时通过全局计数器完成 (Progress updates are handled via global counter during file processing)
        
        return local_count;
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
    
    // 统计完成后显示最终的文件总数 (Show final total file count after counting)
    if (progress_callback && global_file_count > 0) {
        progress_callback(0, global_file_count, CALCULATE_FILES, false, 0.0f);
    }
    
    if (total_files == 0) {
        if (error_callback) {
            error_callback(FILE_NONE);
        }
        return false;
    }


    // // 对cached_files进行浅度排序，确保同目录文件相邻 (Sort cached_files by shallow depth, ensuring files in same directory are adjacent)
    // auto sort_start = std::chrono::high_resolution_clock::now();
    
    // // 自定义排序比较函数：按目录路径分组，实现完全目录聚集 (Custom sort comparator: group by directory path, achieve complete directory clustering)
    // std::sort(cached_files.begin(), cached_files.end(), [](const FileInfo& a, const FileInfo& b) {
    //     // 提取目录路径 (Extract directory paths)
    //     std::string dir_a = a.target_path.substr(0, a.target_path.find_last_of('/'));
    //     std::string dir_b = b.target_path.substr(0, b.target_path.find_last_of('/'));
        
    //     // 如果目录不同，按目录路径排序 (If directories differ, sort by directory path)
    //     if (dir_a != dir_b) {
    //         return dir_a < dir_b;
    //     }
        
    //     // 同目录内直接按完整路径排序，确保目录聚集 (Same directory, sort by full path to ensure directory clustering)
    //     return a.target_path < b.target_path;
    // });
    
    // auto sort_end = std::chrono::high_resolution_clock::now();
    // auto sort_duration = std::chrono::duration_cast<std::chrono::milliseconds>(sort_end - sort_start);
    
    // utils::Logger::Info("文件排序完成，耗时: " + std::to_string(sort_duration.count()) + "ms");
    
    // // 打印排序后的cached_files路径顺序用于分析 (Print sorted cached_files path order for analysis)
    // utils::Logger::Info("=== 排序后文件顺序分析 ===");
    // for (size_t i = 0; i < cached_files.size(); ++i) {
    //     utils::Logger::Info("[" + std::to_string(i) + "] 目标: " + cached_files[i].target_path);
    // }
    // utils::Logger::Info("=== 总计 " + std::to_string(cached_files.size()) + " 个文件 ===");

    // return false;


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

// 非顺序写入，比copyFilesBatch2快20-30s
// 批量文件复制函数 - 优化版本，减少文件句柄开关和缓冲区分配
bool ModManager::copyFilesBatch(const std::vector<FileInfo>& file_info_list,
                               ProgressCallback progress_callback,
                               ErrorCallback error_callback,
                               std::stop_token stop_token) {

    int copied_files = 0; // 已复制文件数量 (Number of copied files)
    int total_files = file_info_list.size();
    
    // SD卡块对齐优化策略 (SD Card Block Alignment Optimization Strategy)
    const size_t SD_BLOCK_SIZE = 64 * 1024; // 64KB SD卡块大小
    const size_t ALIGNED_BUFFER_SIZE = 32 * 1024 * 1024; // 16MB 对齐缓冲区 (256个块)
    const size_t SMALL_FILE_THRESHOLD = 8 * 1024 * 1024;   // 8MB 小文件阈值
    const size_t BATCH_MEMORY_LIMIT = 32 * 1024 * 1024; // 32MB 批量内存限制
    
    
    // 分配块对齐缓冲区 (Allocate block-aligned buffers)
    // Switch平台使用memalign进行内存对齐到SD卡块边界
    char* aligned_buffer = (char*)memalign(SD_BLOCK_SIZE, ALIGNED_BUFFER_SIZE + SD_BLOCK_SIZE);
    
    if (!aligned_buffer) {
        if (error_callback) {
            error_callback("内存分配失败 (Memory allocation failed)");
        }
        return false;
    }
    
    /* SD卡优化策略总结 (SD Card Optimization Strategy Summary):
     * 1. 块对齐写入：所有写入操作对齐到64KB块边界，避免跨块写入
     * 2. 顺序I/O优化：按文件路径排序，减少随机访问模式
     * 3. 批量连续写入：小文件合并写入，减少写入次数
     * 4. 减少fsync频率：批量刷新降低写入放大
     * 5. 内存对齐：缓冲区对齐到块边界，提高传输效率
     * 6. 块级缓存：使用64KB整数倍缓冲区，匹配SD卡特性
     * 7. 写入地址对齐：确保文件写入位置对齐到块边界
     * 8. 智能填充：不足块大小的数据进行零填充对齐
     */
    
    
    
    bool overall_success = true;
    
    // 优化：单次遍历处理，小文件累积到内存池，大文件立即处理 (Optimization: single-pass processing, accumulate small files in memory pool, process large files immediately)
    std::vector<std::pair<std::string, std::vector<char>>> cached_files;
    size_t total_cached_size = 0;
    
    // 批量写入缓存小文件的辅助函数 (Helper function to batch write cached small files)
    auto flush_cached_files = [&]() -> bool {
        for (const auto& cached : cached_files) {
            FILE* dest_file = fopen(cached.first.c_str(), "wb");
            if (dest_file) {
                // SD卡块对齐写入优化 (SD Card block-aligned write optimization)
                setvbuf(dest_file, nullptr, _IOFBF, ALIGNED_BUFFER_SIZE);
                
                size_t written = fwrite(cached.second.data(), 1, cached.second.size(), dest_file);
                fclose(dest_file);
                
                if (written == cached.second.size()) {
                    copied_files++;
                    if (progress_callback) {
                        size_t last_slash = cached.first.find_last_of('/');
                        const char* filename_ptr = (last_slash != std::string::npos) ? 
                            cached.first.c_str() + last_slash + 1 : cached.first.c_str();
                        progress_callback(copied_files, total_files, filename_ptr, false, 0.0f);
                    }
                } else {
                    if (error_callback) {
                        error_callback(CANT_WRITE_ERROR + cached.first);
                    }
                    return false;
                }
            } else {
                if (error_callback) {
                    error_callback(CANT_CREATE_DIR + cached.first);
                }
                return false;
            }
        }
        cached_files.clear();
        total_cached_size = 0;
        return true;
    };
    
    // 单次遍历处理所有文件 (Single-pass processing of all files)
    for (const auto& file_info : file_info_list) {
        // 检查是否需要停止 (Check if stop is requested)
        if (stop_token.stop_requested()) {
            free(aligned_buffer);
            return false;
        }
        
        if (file_info.file_size <= SMALL_FILE_THRESHOLD) {
             // 处理小文件：累积到内存池 (Process small files: accumulate in memory pool)
             
             // 检查内存限制，如果超出则先写入已缓存的文件 (Check memory limit, flush cached files if exceeded)
             if (total_cached_size + file_info.file_size > BATCH_MEMORY_LIMIT) {
                 if (!flush_cached_files()) {
                     free(aligned_buffer);
                     return false;
                 }
             }
             
             FILE* source_file = fopen(file_info.source_path.c_str(), "rb");
             if (!source_file) {
                 if (error_callback) {
                     error_callback(CANT_OPEN_FILE + file_info.source_path);
                 }
                 free(aligned_buffer);
                 return false;
             }
            
            // SD卡块对齐读取优化 (SD Card block-aligned read optimization)
             size_t file_size = file_info.file_size;
             size_t aligned_size = ((file_size + SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE) * SD_BLOCK_SIZE;
             std::vector<char> file_data;
             file_data.reserve(aligned_size);
             file_data.resize(aligned_size, 0);
             
             setvbuf(source_file, nullptr, _IOFBF, ALIGNED_BUFFER_SIZE);
             
             size_t bytes_read = fread(file_data.data(), 1, file_size, source_file);
             fclose(source_file);
             
             if (bytes_read == file_size) {
                 cached_files.push_back({file_info.target_path, std::move(file_data)});
                 total_cached_size += aligned_size;
                 
                 if (progress_callback) {
                     progress_callback(copied_files, total_files, Aggregating_Files, false, 0.0f);
                 }
             } else {
                 if (error_callback) {
                     error_callback(CANT_READ_ERROR + file_info.source_path);
                 }
                 free(aligned_buffer);
                 return false;
             }
         } else {
             // 处理大文件：先写入所有缓存的小文件，然后立即处理大文件 (Process large files: flush all cached small files first, then process large file immediately)
             if (!cached_files.empty()) {
                 if (!flush_cached_files()) {
                     free(aligned_buffer);
                     return false;
                 }
             }
             
             // 打开源文件 (Open source file)
             FILE* source_file = fopen(file_info.source_path.c_str(), "rb");
             if (!source_file) {
                 if (error_callback) {
                     error_callback(CANT_OPEN_FILE + file_info.source_path);
                 }
                 free(aligned_buffer);
                 return false;
             }
             
             // SD卡块对齐缓冲区优化 (SD Card block-aligned buffer optimization)
             setvbuf(source_file, nullptr, _IOFBF, ALIGNED_BUFFER_SIZE);
             
             long file_size = static_cast<long>(file_info.file_size);
             
             // 打开目标文件 (Open destination file)
             FILE* dest_file = fopen(file_info.target_path.c_str(), "wb");
             if (!dest_file) {
                 fclose(source_file);
                 if (error_callback) {
                     error_callback(CANT_CREATE_DIR + file_info.target_path + ", errno: " + std::to_string(errno));
                 }
                 free(aligned_buffer);
                 return false;
             }
             
             // SD卡块对齐写入缓冲区 (SD Card block-aligned write buffer)
             setvbuf(dest_file, nullptr, _IOFBF, ALIGNED_BUFFER_SIZE);
             
             bool file_success = true;
             
             // 提前提取文件名，避免重复计算 (Extract filename early to avoid repeated calculation)
             size_t last_slash = file_info.source_path.find_last_of('/');
             const char* filename_ptr = (last_slash != std::string::npos) ? 
                 file_info.source_path.c_str() + last_slash + 1 : file_info.source_path.c_str();
             
             // SD卡块对齐分块复制大文件 (SD Card block-aligned chunked copy of large files)
             size_t total_read = 0;
             int last_progress = -1;
             
             while (total_read < (size_t)file_size && file_success) {
                 // 在文件复制过程中检查是否需要停止 (Check if stop is requested during file copy)
                 if (stop_token.stop_requested()) {
                     fclose(source_file);
                     fclose(dest_file);
                     free(aligned_buffer); // 使用free释放memalign分配的内存
                     return false;
                 }
                 
                 // 计算块对齐的读取大小 (Calculate block-aligned read size)
                 size_t remaining = (size_t)file_size - total_read;
                 size_t to_read = std::min(ALIGNED_BUFFER_SIZE, remaining);
                 
                 // 如果是最后一块且不足块大小，对齐到块边界 (If last chunk and less than block size, align to block boundary)
                 size_t aligned_read = to_read;
                 if (remaining < ALIGNED_BUFFER_SIZE && (to_read % SD_BLOCK_SIZE) != 0) {
                     aligned_read = ((to_read + SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE) * SD_BLOCK_SIZE;
                     // 清零填充区域 (Zero-fill padding area)
                     memset(aligned_buffer + to_read, 0, aligned_read - to_read);
                 }
                 
                 size_t bytes_read = fread(aligned_buffer, 1, to_read, source_file);
                 if (bytes_read > 0) {
                     // 写入实际读取的字节数，不进行块对齐填充 (Write actual bytes read, no block alignment padding)
                     if (fwrite(aligned_buffer, 1, bytes_read, dest_file) != bytes_read) {
                         file_success = false;
                         break;
                     }
                     total_read += bytes_read;
                     
                     // 更新文件级进度 - 仅对大文件显示 (Update file-level progress - only show for large files)
                     if (progress_callback && file_size > 8 * 1024 * 1024) {
                         int progress_percent = (int)((total_read * 100) / file_size);
                         // 实时更新进度，不限制更新频率 (Real-time progress update without frequency limitation)
                         progress_callback(copied_files, total_files, filename_ptr, true, (float)progress_percent);
                         last_progress = progress_percent;
                     }
                 }
                 if (bytes_read < to_read) break;
             }
              
              // 统一的错误处理 (Unified error handling)
              if (!file_success && error_callback) {
                  if (ferror(source_file)) {
                      error_callback(CANT_READ_ERROR + file_info.source_path);
                  } else if (ferror(dest_file)) {
                      error_callback(CANT_WRITE_ERROR + file_info.target_path);
                  } else {
                      error_callback(CANT_WRITE_ERROR + file_info.target_path);
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
     }
     
     // 处理剩余的小文件缓存 (Process remaining cached small files)
     if (!flush_cached_files()) {
         overall_success = false;
     }
     
     // 清理块对齐缓冲区 (Clean up block-aligned buffers)
     free(aligned_buffer); // 使用free释放memalign分配的内存
     
     return overall_success;
}

// 批量复制文件（简化版本，不聚集小文件，直接按顺序写入），不如带聚合的，备用不删了
bool ModManager::copyFilesBatch2(const std::vector<FileInfo>& file_info_list,
                                ProgressCallback progress_callback,
                                ErrorCallback error_callback,
                                std::stop_token stop_token) {

    int copied_files = 0; // 已复制文件数量 (Number of copied files)
    int total_files = file_info_list.size();
    
    // SD卡块对齐优化策略 (SD Card Block Alignment Optimization Strategy)
    const size_t SD_BLOCK_SIZE = 64 * 1024; // 64KB SD卡块大小
    const size_t ALIGNED_BUFFER_SIZE = 32 * 1024 * 1024; // 32MB 对齐缓冲区
    
    // 分配块对齐缓冲区 (Allocate block-aligned buffers)
    // Switch平台使用memalign进行内存对齐到SD卡块边界
    char* aligned_buffer = (char*)memalign(SD_BLOCK_SIZE, ALIGNED_BUFFER_SIZE + SD_BLOCK_SIZE);
    
    if (!aligned_buffer) {
        if (error_callback) {
            error_callback("内存分配失败 (Memory allocation failed)");
        }
        return false;
    }
    
    /* SD卡优化策略总结 (SD Card Optimization Strategy Summary):
     * 1. 块对齐写入：所有写入操作对齐到64KB块边界，避免跨块写入
     * 2. 顺序I/O优化：按文件路径排序，减少随机访问模式
     * 3. 直接写入：不聚集小文件，每个文件直接按顺序写入
     * 4. 内存对齐：缓冲区对齐到块边界，提高传输效率
     * 5. 块级缓存：使用64KB整数倍缓冲区，匹配SD卡特性
     * 6. 写入地址对齐：确保文件写入位置对齐到块边界
     * 7. 智能填充：不足块大小的数据进行零填充对齐
     */
    
    bool overall_success = true;
    
    // 直接按顺序处理所有文件 (Process all files directly in order)
    for (const auto& file_info : file_info_list) {
        // 检查是否需要停止 (Check if stop is requested)
        if (stop_token.stop_requested()) {
            free(aligned_buffer);
            return false;
        }
        
        // 打开源文件 (Open source file)
        FILE* source_file = fopen(file_info.source_path.c_str(), "rb");
        if (!source_file) {
            if (error_callback) {
                error_callback(CANT_OPEN_FILE + file_info.source_path);
            }
            free(aligned_buffer);
            return false;
        }
        
        // SD卡块对齐缓冲区优化 (SD Card block-aligned buffer optimization)
        setvbuf(source_file, nullptr, _IOFBF, ALIGNED_BUFFER_SIZE);
        
        long file_size = static_cast<long>(file_info.file_size);
        
        // 打开目标文件 (Open destination file)
        FILE* dest_file = fopen(file_info.target_path.c_str(), "wb");
        if (!dest_file) {
            fclose(source_file);
            if (error_callback) {
                error_callback(CANT_CREATE_DIR + file_info.target_path + ", errno: " + std::to_string(errno));
            }
            free(aligned_buffer);
            return false;
        }
        
        // SD卡块对齐写入缓冲区 (SD Card block-aligned write buffer)
        setvbuf(dest_file, nullptr, _IOFBF, ALIGNED_BUFFER_SIZE);
        
        bool file_success = true;
        
        // 提前提取文件名，避免重复计算 (Extract filename early to avoid repeated calculation)
        size_t last_slash = file_info.source_path.find_last_of('/');
        const char* filename_ptr = (last_slash != std::string::npos) ? 
            file_info.source_path.c_str() + last_slash + 1 : file_info.source_path.c_str();
        
        // SD卡块对齐分块复制文件 (SD Card block-aligned chunked copy of files)
        size_t total_read = 0;
        int last_progress = -1;
        
        while (total_read < (size_t)file_size && file_success) {
            // 在文件复制过程中检查是否需要停止 (Check if stop is requested during file copy)
            if (stop_token.stop_requested()) {
                fclose(source_file);
                fclose(dest_file);
                free(aligned_buffer); // 使用free释放memalign分配的内存
                return false;
            }
            
            // 计算块对齐的读取大小 (Calculate block-aligned read size)
            size_t remaining = (size_t)file_size - total_read;
            size_t to_read = std::min(ALIGNED_BUFFER_SIZE, remaining);
            
            // 如果是最后一块且不足块大小，对齐到块边界 (If last chunk and less than block size, align to block boundary)
            size_t aligned_read = to_read;
            if (remaining < ALIGNED_BUFFER_SIZE && (to_read % SD_BLOCK_SIZE) != 0) {
                aligned_read = ((to_read + SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE) * SD_BLOCK_SIZE;
                // 清零填充区域 (Zero-fill padding area)
                memset(aligned_buffer + to_read, 0, aligned_read - to_read);
            }
            
            size_t bytes_read = fread(aligned_buffer, 1, to_read, source_file);
            if (bytes_read > 0) {
                // SD卡块对齐写入 (SD Card block-aligned write)
                size_t write_size = (remaining < ALIGNED_BUFFER_SIZE && (bytes_read % SD_BLOCK_SIZE) != 0) ? aligned_read : bytes_read;
                
                if (fwrite(aligned_buffer, 1, write_size, dest_file) != write_size) {
                    file_success = false;
                    break;
                }
                total_read += bytes_read;
                
                // 更新文件级进度 (Update file-level progress)
                if (progress_callback && file_size > 8 * 1024 * 1024) { // 只对大于8MB的文件显示文件级进度
                    int progress_percent = (int)((total_read * 100) / file_size);
                    // 实时更新进度，不限制更新频率 (Real-time progress update without frequency limitation)
                    progress_callback(copied_files, total_files, filename_ptr, true, (float)progress_percent);
                    last_progress = progress_percent;
                }
            }
            if (bytes_read < to_read) break;
        }
         
         // 统一的错误处理 (Unified error handling)
         if (!file_success && error_callback) {
             if (ferror(source_file)) {
                 error_callback(CANT_READ_ERROR + file_info.source_path);
             } else if (ferror(dest_file)) {
                 error_callback(CANT_WRITE_ERROR + file_info.target_path);
             } else {
                 error_callback(CANT_WRITE_ERROR + file_info.target_path);
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
    
    // 清理块对齐缓冲区 (Clean up block-aligned buffers)
    free(aligned_buffer); // 使用free释放memalign分配的内存
    
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

// 接受一个mod目录的路径，检查其合法性（检查是否为单个ZIP文件）
std::string ModManager::Removemodechecklegality(const std::string& mod_dir_path) {
    // 检查路径是否为空 (Check if path is empty)
    if (mod_dir_path.empty()) {
        return "";
    }
    
    // 打开目录 (Open directory)
    DIR* dir = opendir(mod_dir_path.c_str());
    if (dir == nullptr) {
        return ""; // 无法打开目录 (Cannot open directory)
    }
    
    std::string mod_zip_path = "";
    
    // 遍历目录 (Traverse directory)
    struct dirent* entry;
    int count = 0; // 计数器 (Counter)
    
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过 . 和 .. (Skip . and ..)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        count++; // 次数计数器+1 (Counter increment)

        // 检查计数器大小，只要大于1，就代表不合法
        if (count > 1) {
            mod_zip_path = "";
            break;
        }
        
        // 检查是否为文件 (Check if it's a file)
        if (entry->d_type != DT_REG) {
            
            break;
        }
        
        // 检查是否为ZIP文件 (Check if it's a ZIP file)
        std::string filename = entry->d_name;
        size_t len = filename.length();

        if (len <= 4) {
            break;
        }
        
        // 获取文件扩展名 (Get file extension)
        std::string ext = filename.substr(len - 4);
        
        // 只对比 .ZIP 和 .zip 两种情况 (Only compare .ZIP and .zip)
        if (ext != ".zip" && ext != ".ZIP") {
            break;
        } else {
            mod_zip_path = mod_dir_path + "/" + filename;
        }
            
        
    }
    
    closedir(dir);
    
    return mod_zip_path;
}

// 接受一个zip文件的路径，将这个zip文件移动到/mods2/0000-add-mod/目录下
bool ModManager::Removemodesingezipmod(const std::string& mod_zip_path) {
    // 提取文件名
    size_t pos = mod_zip_path.find_last_of("/\\");
    std::string filename = (pos != std::string::npos) ? mod_zip_path.substr(pos + 1) : mod_zip_path;
    
    // 构建目标路径
    std::string target_path = "/mods2/0000-add-mod-0000/" + filename;

    for (int i = 0; i < 20; ++i) {
        // 检查目标文件是否存在 (Check if target file exists)
        struct stat st;
        if (stat(target_path.c_str(), &st) == 0) {
            // 文件存在，生成新的文件名 (File exists, generate new filename)
            target_path = "/mods2/0000-add-mod-0000/" + std::to_string(i + 1) + "-" + filename;
            continue;
        }

        // 尝试重命名 (Try to rename)
        if (rename(mod_zip_path.c_str(), target_path.c_str()) == 0) {
            return true; // 成功 (Success)
        } else {
            return false; // 重命名失败 (Rename failed)
        }
    }

    return false;
    
}

std::string ModManager::GetModJsonPath(const std::string& mod_dir_path) {
    // 直接截取到最后一个斜杠位置，拼接mod_name.json
    size_t last_slash = mod_dir_path.find_last_of("/\\");
    std::string mod_json_path = mod_dir_path.substr(0, last_slash) + "/mod_name.json";
    return mod_json_path;
}

std::string ModManager::GetModDirName(const std::string& mod_dir_path) {
    std::string mod_dir_name = mod_dir_path.substr(mod_dir_path.find_last_of("/\\") + 1);
    return mod_dir_name;
}

// 从FILE_PATH中提取游戏目录名
std::string ModManager::GetGameDirName(const std::string& game_file_path) {
    // 固定格式为/mods2/游戏名字/ID，直接提取游戏名字部分
    std::string path_without_prefix = game_file_path.substr(7); // 跳过"/mods2/"
    size_t slash_pos = path_without_prefix.find('/'); // 查找第一个斜杠位置
    return path_without_prefix.substr(0, slash_pos); // 返回游戏名字
}

// 接受MOD目录路径，一个错误回调函数，在这里负责处理错误反馈和remove调度
bool ModManager::Removemodeformodlist(const std::string& mod_dir_path,ErrorCallback error_callback) {

    // 检查合法性，是否为单个ZIP文件，不合法返回空
    std::string mod_zip_path = Removemodechecklegality(mod_dir_path);
    if (mod_zip_path.empty()) {
        if (error_callback) {
            error_callback(NON_ZIP_MOD_FOUND + mod_dir_path);
        }
        return false;
    }

    // 尝试移动到0000-add-mod-0000目录下
    if (!Removemodesingezipmod(mod_zip_path)) {
        if (error_callback) {
            error_callback(MOVE_MOD_FAILED + mod_zip_path);
        }
        return false;
    }

    // 尝试删除mod目录,失败不管他。
    if (remove(mod_dir_path.c_str()) != 0) {
        // if (error_callback) {
        //     error_callback("删除MOD目录失败，请检查！\n" + mod_dir_path);
        // }
        // return false;
    }


    std::string mod_json_path = GetModJsonPath(mod_dir_path);
    std::string mod_dir_name = GetModDirName(mod_dir_path);

    // 尝试删除modjson文件中的键（包含值),键就是mod目录名,失败不管他。
    tj::JsonManager::RemoveRootJsonKeyValue(mod_json_path, mod_dir_name);

    return true;
    
}

std::vector<std::string> ModManager::GetGameAllModPath(const std::string& game_file_path) {
    
    // 遍历FILE_PATH路径下所有目录，生成包含所有目录的数组
    std::vector<std::string> directories;
    
    DIR* dir = opendir(game_file_path.c_str());
    if (dir != nullptr) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            // 跳过当前目录和父目录
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            // 使用entry的d_type成员检测是否为目录
            if (entry->d_type == DT_DIR) {
                // 检查entry->d_name的最后一位字符是否是'$',如果是check_mod_state变成false
                std::string dir_name = entry->d_name;
                if (!dir_name.empty() && dir_name.back() == '$') {
                    closedir(dir);
                    return {};
                }
                directories.push_back(game_file_path + "/" + dir_name);
            }
        }
        closedir(dir);
    }

    return directories;
    
}


bool ModManager::Removegameandallmods(const std::string& game_file_path,ErrorCallback error_callback) {

    std::vector<std::string> directories = GetGameAllModPath(game_file_path);
    
    // 检查是否为空
    if (directories.empty()) {
        // 调用错误回调函数
        if (error_callback) {
            error_callback(HAVE_UNINSTALLED_MOD);
        }
        return false;
    }

    // 用来判断是否全部成功
    int count = 0;

    //  /mods2/游戏名字/ID/mod_name.json
    std::string mod_json_path = game_file_path + "/mod_name.json";

    //  /mods2/游戏名字
    size_t second_last_slash = game_file_path.find_last_of('/');
    std::string game_name_path = game_file_path.substr(0, second_last_slash);  // 提取到游戏名字路径
    

    // 先遍历一遍接收到的目录，尝试都移除
    for (const auto& dir_path : directories) {

        // 检查目录是否合法，不合法就跳过到下一个
        std::string mod_zip_path = Removemodechecklegality(dir_path);
        if (mod_zip_path.empty()) {
            count++;
            continue;
        }

        // 尝试移除目录下的ZIPmod，删除失败就跳过
        if (!Removemodesingezipmod(mod_zip_path)) {
            count++;
            continue;
        }

        // 尝试删除目录，删除失败就跳过，不计数
        if (remove(dir_path.c_str()) != 0) {
            continue;
        }

        // 根据固定格式 /mods2/游戏名字/ID/模组的名字 解析路径
        // 提取 /mods2/游戏名字/ID/ 作为 mod_json_path 的基础路径
        size_t last_slash = dir_path.find_last_of('/');
        std::string root_key = dir_path.substr(last_slash + 1);     // 模组的名字

        // 调用JsonManager删除modjson中对应的根级键值对，失败不管
        tj::JsonManager::RemoveRootJsonKeyValue(mod_json_path, root_key);

    }

    // 检查是否全部成功
    if (count == 0) {
        // 删除modjson文件，游戏路径，失败不管。
        remove(mod_json_path.c_str());
        remove(game_file_path.c_str());
        remove(game_name_path.c_str());
        std::string game_dir_name = GetGameDirName(game_file_path);
        // 删除game_name.json中的游戏映射名，不管失败
        tj::JsonManager::RemoveNestedJsonKeyValue("/mods2/game_name.json", "favorites$180", game_dir_name); 
        tj::JsonManager::RemoveRootJsonKeyValue("/mods2/game_name.json", game_dir_name); 
        return true;
    }

    // 调用错误回调函数
    if (error_callback) {
        error_callback(REMOVE_GAME_ERROR + std::to_string(count));
    }

    return false;
}
