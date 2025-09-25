#pragma once

#include <string>
#include <vector>
#include <functional>
#include <stop_token>
#include <switch.h>  // 包含Switch平台的类型定义，如u32

/**
 * MOD管理器类
 * 负责MOD的压缩、解压和安装操作
 */
class ModManager {
public:



    static const std::string target_directory_zip;


    // 进度回调函数类型 (当前进度, 总数, 当前文件名, 是否正在复制文件, 文件级进度百分比)
    using ProgressCallback = std::function<void(int current, int total, const std::string& filename,
                                                bool is_copying_file, float file_progress_percentage,
                                                const std::string& dialog_title, const int* progress_bar_color)>;
    
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
                     std::vector<int>& files_to_extract,
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
    bool RemoveModFilesFromCache(const std::string& mod_common_path,
                                ProgressCallback progress_callback,
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
     * 清理已创建的目录
     * @param directories 目录列表
     * @param created_count 已创建的目录数量
     * @param error_callback 错误回调函数
     */
    void cleanupCreatedDirectories(const std::vector<std::string>& directories, 
                                  size_t created_count,
                                  ErrorCallback error_callback);
    
    /**
     * 清理已复制的文件和已创建的目录（用于copyFilesBatch中断时的清理）
     * @param copied_files 已复制的文件路径列表
     * @param progress_callback 进度回调函数
     * @param total_items 总项目数（文件数+目录数）
     */
    void cleanupCopiedFilesAndDirectories(std::vector<std::string>& copied_files,
                                         ProgressCallback progress_callback,
                                         int total_items);


    /**
     * 清理已复制的文件和已创建的目录（用于安装出错时的清理）
     * @param copied_files 已复制的文件路径列表
     * @param progress_callback 进度回调函数
     * @param total_items 总项目数（文件数+目录数）
     */
    void cleanupCopiedFilesAndDirectories_forError(std::vector<std::string>& copied_files,
                                         ProgressCallback progress_callback,
                                         int total_items);

    
    /**
     * 规范化路径（统一路径分隔符）
     * @param path 原始路径
     * @return 规范化后的路径
     */
    std::string normalizePath(const std::string& path);

    std::string GetModJsonPath(const std::string& mod_dir_path);
    std::string GetModFileCommonPath(const std::string& path);
    std::string GetModDirName(const std::string& mod_dir_path);
    std::string GetFilePath(const std::string& path);
    // 从FILE_PATH中提取游戏目录名
    std::string GetGameDirName(const std::string& game_file_path);

    std::vector<std::string> GetGameAllModPath(const std::string& game_file_path);
    
    bool Removemodeformodlist(const std::string& mod_dir_path,ErrorCallback error_callback);
    std::string Removemodechecklegality(const std::string& mod_dir_path);
    bool Removemodesingezipmod(const std::string& mod_zip_path);
    bool Removegameandallmods(const std::string& game_file_path,ErrorCallback error_callback);

    std::vector<std::string> GetAllModDirPaths(const std::string& game_file_path);
    std::string GetModJsonName(const std::string& mod_dir_path);

    void GetConflictingModNames(const std::string& mod_dir_path,const std::string& conflicting_file_Path,
                                ProgressCallback progress_callback,ErrorCallback error_callback,std::stop_token stop_token);

    void CachedConflictingFiles(const std::string& path,ProgressCallback progress_callback);

    /**
     * 使用libnx硬件加速计算文件的CRC32值
     * @param file_path 文件路径
     * @return 文件的CRC32值，失败时返回0
     */
    u32 GetFileCrc32(const char* file_path);

private:
    // 缓存的目标文件路径列表，用于卸载时直接删除 (Cached target file paths for direct deletion during uninstall)
    std::vector<std::string> cached_target_files;
    
    // 缓存的已创建目录路径列表，用于清理时删除空目录 (Cached created directory paths for cleanup of empty directories)
    std::vector<std::string> cached_created_directories;

    // 缓存发生冲突且通过CRC32校验的目标文件路径 (Cached target file paths that conflict and pass CRC32 check)
    std::vector<std::string> cached_conflicting_files;
    
    // 临时增加两个变量，用于进度条颜色，后面重构一下回调函数，改成结构体
    static const int COLOR_BLUE[3];
    static const int COLOR_RED[3];
};