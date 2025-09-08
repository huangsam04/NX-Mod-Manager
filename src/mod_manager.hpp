#pragma once

#include <string>
#include <vector>
#include <functional>
#include <stop_token>

/**
 * MOD管理器类
 * 负责MOD的压缩、解压和安装操作
 */
class ModManager {
public:



    static const std::string target_directory_zip;


    // 进度回调函数类型 (当前进度, 总数, 当前文件名, 是否正在复制文件, 文件级进度百分比)
    using ProgressCallback = std::function<void(int current, int total, const std::string& filename, bool is_copying_file, float file_progress_percentage)>;
    
    // 错误回调函数类型
    using ErrorCallback = std::function<void(const std::string& error_message)>;
    
    /**
     * 构造函数
     */
    ModManager();
    
    /**
     * 析构函数
     */
    ~ModManager();
    

    
    /**
     * 解压ZIP文件到目标目录（简化版本，不使用小文件聚合，直接按顺序写入SD卡）
     * 注意：此函数必须接收已初始化的ZIP读取器，不会自行创建ZIP读取器
     * @param zip_path ZIP文件路径（用于错误信息显示）
     * @param progress_callback 进度回调函数
     * @param error_callback 错误回调函数
     * @param stop_token 停止令牌，用于中断操作
     * @param zip_archive_ptr 已初始化的mz_zip_archive指针（必须非空）
     * @return 成功返回true，失败返回false
     */
    bool extractMod(const std::string& zip_path,
                     int& files_total,
                     ProgressCallback progress_callback = nullptr,
                     ErrorCallback error_callback = nullptr,
                     std::stop_token stop_token = {},
                     void* zip_archive_ptr = nullptr);
    

    /**
     * 解压ZIP文件到目标目录（简化版本，不使用小文件聚合，直接按顺序写入SD卡）
     * @param zip_path ZIP文件路径
     * @param progress_callback 进度回调函数
     * @param error_callback 错误回调函数
     * @param stop_token 停止令牌，用于中断操作
     * @return 成功返回true，失败返回false
     */
    bool extractMod2(const std::string& zip_path,
                     ProgressCallback progress_callback = nullptr,
                     ErrorCallback error_callback = nullptr,
                     std::stop_token stop_token = {});    

    
    /**
     * 直接解压ZIP文件到atmosphere目录（不复制，直接解压）
     * @param zip_path ZIP文件路径
     * @param progress_callback 进度回调函数
     * @param error_callback 错误回调函数
     * @param stop_token 停止令牌，用于中断操作
     * @return 成功返回true，失败返回false
     */
    bool installModFromZipDirect(const std::string& zip_path,
                                ProgressCallback progress_callback = nullptr,
                                ErrorCallback error_callback = nullptr,
                                std::stop_token stop_token = {});
    
    /**
     * 安装文件夹类型的MOD（与app中现有方法相同的逻辑）
     * @param folder_path 文件夹路径
     * @param progress_callback 进度回调函数
     * @param error_callback 错误回调函数
     * @param stop_token 停止令牌，用于中断操作
     * @return 成功返回true，失败返回false
     */
    bool installModFromFolder(const std::string& folder_path,
                             ProgressCallback progress_callback = nullptr,
                             ErrorCallback error_callback = nullptr,
                             std::stop_token stop_token = {});
    
    /**
     * 复制单个文件
     * @param source_path 源文件路径
     * @param dest_path 目标文件路径
     * @param error_callback 错误回调函数
     * @param progress_callback 进度回调函数
     * @param current_file_index 当前文件索引（用于总进度显示）
     * @param total_files 总文件数（用于总进度显示）
     * @return 成功返回true，失败返回false
     */

    
    /**
     * 批量复制文件（优化版本，减少文件句柄开关次数）
     * @param file_pairs 源文件和目标文件路径对的向量
     * @param progress_callback 进度回调函数
     * @param error_callback 错误回调函数
     * @param stop_token 停止令牌，用于中断操作
     * @return 成功返回true，失败返回false
     */
    // 文件信息结构体 (File info structure)
    struct FileInfo {
        std::string source_path;  // 源路径 (Source path)
        std::string target_path;  // 目标路径 (Target path)
        size_t file_size;         // 文件大小 (File size)
    };
    
    bool copyFilesBatch(const std::vector<FileInfo>& file_info_list,
                         ProgressCallback progress_callback = nullptr,
                         ErrorCallback error_callback = nullptr,
                         std::stop_token stop_token = {});
    
    /**
     * 批量复制文件（简化版本，不聚集小文件，直接按顺序写入）
     * @param file_info_list 文件信息列表
     * @param progress_callback 进度回调函数
     * @param error_callback 错误回调函数
     * @param stop_token 停止令牌，用于中断操作
     * @return 成功返回true，失败返回false
     */
    bool copyFilesBatch2(const std::vector<FileInfo>& file_info_list,
                          ProgressCallback progress_callback = nullptr,
                          ErrorCallback error_callback = nullptr,
                          std::stop_token stop_token = {});
    
    /**
     * 卸载文件夹类型的MOD（删除atmosphere目录中对应的文件）
     * @param folder_path MOD文件夹路径
     * @param progress_callback 进度回调函数
     * @param error_callback 错误回调函数
     * @param stop_token 停止令牌，用于中断操作
     * @return 成功返回true，失败返回false
     */
    bool uninstallModFromFolder(const std::string& folder_path,
                               ProgressCallback progress_callback = nullptr,
                               ErrorCallback error_callback = nullptr,
                               std::stop_token stop_token = {});
    
    /**
     * 卸载ZIP类型的MOD（删除atmosphere目录中对应的文件）
     * @param zip_path ZIP文件路径
     * @param progress_callback 进度回调函数
     * @param error_callback 错误回调函数
     * @param stop_token 停止令牌，用于中断操作
     * @return 成功返回true，失败返回false
     */
    bool uninstallModFromZipDirect(const std::string& zip_path,
                                  ProgressCallback progress_callback = nullptr,
                                  ErrorCallback error_callback = nullptr,
                                  std::stop_token stop_token = {});
    
    /**
     * 判断MOD安装类型并直接启动安装
     * @param mod_path MOD路径
     * @param operation_type 操作类型：0=卸载，1=删除
     * @param progress_callback 进度回调函数
     * @param error_callback 错误回调函数
     * @param stop_token 停止令牌，用于中断操作
     * @return 成功返回true，失败返回false
     */
    bool getModInstallType(const std::string& mod_path,
                          int operation_type,
                          ProgressCallback progress_callback = nullptr,
                          ErrorCallback error_callback = nullptr,
                          std::stop_token stop_token = {});
    
    /**
     * 统计需要删除的文件数量并缓存路径
     * @param mod_source_path MOD源路径
     * @param progress_callback 进度回调函数
     * @param error_callback 错误回调函数
     * @param current_counted 当前已统计的文件数量指针
     * @return 统计到的文件数量
     */
    size_t CountModFilesToRemoveWithProgress(const std::string& mod_source_path,
                                            ProgressCallback progress_callback,
                                            ErrorCallback error_callback,
                                            size_t* current_counted,
                                            std::stop_token stop_token);
    
    /**
     * 基于缓存路径删除文件
     * @param progress_callback 进度回调函数
     * @param error_callback 错误回调函数
     * @return 成功返回true，失败返回false
     */
    bool RemoveModFilesFromCache(ProgressCallback progress_callback,
                                ErrorCallback error_callback,
                                std::stop_token stop_token);
    
    /**
     * 批量创建目录
     * @param directories 待创建的目录路径列表
     * @param progress_callback 进度回调函数
     * @param total_files 总文件数
     * @param error_callback 错误回调函数
     * @return 成功返回true，失败返回false
     */
    bool createDirectoriesBatch(std::vector<std::string>& directories, 
                               ProgressCallback progress_callback, 
                               size_t total_files,
                               ErrorCallback error_callback,
                               std::stop_token stop_token);
    
    /**
     * 规范化路径（统一路径分隔符）
     * @param path 原始路径
     * @return 规范化后的路径
     */
    std::string normalizePath(const std::string& path);

private:
    // 缓存的目标文件路径列表，用于卸载时直接删除 (Cached target file paths for direct deletion during uninstall)
    std::vector<std::string> cached_target_files;
};