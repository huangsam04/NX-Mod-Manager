
// MTP有BUG，很难受，懒得细搞了，瞎逼弄弄拉倒
// MTP传输大点文件容易卡死

#pragma once

#include <switch.h>
#include <vector>
#include <memory>
#include <mutex>
#include "haze.h"
#include "lang_manager.hpp"

namespace mtp {

// MTP传输状态枚举
enum class MtpStatus {
    Stopped,        // 已停止
    Starting,       // 启动中
    Running,        // 运行中
    Stopping        // 停止中
};

// MTP传输状态信息（直接存储格式化的状态文本）
// 例如："[正在读取]：文件名.txt" 或 "MTP已开启，请用USB线连接电脑"

// SD卡文件系统代理类
class SdCardFileSystemProxy : public haze::FileSystemProxyImpl {
public:
    SdCardFileSystemProxy();
    ~SdCardFileSystemProxy();

    // 基本信息
    const char* GetName() const override;
    const char* GetDisplayName() const override;

    // 空间信息
    Result GetTotalSpace(const char *path, s64 *out) override;
    Result GetFreeSpace(const char *path, s64 *out) override;
    Result GetEntryType(const char *path, FsDirEntryType *out_entry_type) override;

    // 文件操作
    Result CreateFile(const char* path, s64 size, u32 option) override;
    Result DeleteFile(const char* path) override;
    Result RenameFile(const char *old_path, const char *new_path) override;
    Result OpenFile(const char *path, u32 mode, FsFile *out_file) override;
    Result GetFileSize(FsFile *file, s64 *out_size) override;
    Result SetFileSize(FsFile *file, s64 size) override;
    Result ReadFile(FsFile *file, s64 off, void *buf, u64 read_size, u32 option, u64 *out_bytes_read) override;
    Result WriteFile(FsFile *file, s64 off, const void *buf, u64 write_size, u32 option) override;
    void CloseFile(FsFile *file) override;

    // 目录操作
    Result CreateDirectory(const char* path) override;
    Result DeleteDirectoryRecursively(const char* path) override;
    Result RenameDirectory(const char *old_path, const char *new_path) override;
    Result OpenDirectory(const char *path, u32 mode, FsDir *out_dir) override;
    Result ReadDirectory(FsDir *d, s64 *out_total_entries, size_t max_entries, FsDirectoryEntry *buf) override;
    Result GetDirectoryEntryCount(FsDir *d, s64 *out_count) override;
    void CloseDirectory(FsDir *d) override;

    // 多线程传输支持
    bool MultiThreadTransfer(s64 size, bool read) override;

private:
    FsFileSystem* m_fs;     // 文件系统指针
    
    // 路径修正函数
    const char* FixPath(const char* path, char* out = nullptr) const;
};

// ADD MOD文件系统代理类 - 简单实现
class AddModProxy : public haze::FileSystemProxyImpl {
public:
    AddModProxy();
    
    // 基本信息
    const char* GetName() const override;
    const char* GetDisplayName() const override;
    
    // 空间信息
    Result GetTotalSpace(const char *path, s64 *out) override;
    Result GetFreeSpace(const char *path, s64 *out) override;
    Result GetEntryType(const char *path, FsDirEntryType *out_entry_type) override;
    
    // 文件操作
    Result CreateFile(const char* path, s64 size, u32 option) override;
    Result DeleteFile(const char* path) override;
    Result RenameFile(const char *old_path, const char *new_path) override;
    Result OpenFile(const char *path, u32 mode, FsFile *out_file) override;
    Result GetFileSize(FsFile *file, s64 *out_size) override;
    Result SetFileSize(FsFile *file, s64 size) override;
    Result ReadFile(FsFile *file, s64 off, void *buf, u64 read_size, u32 option, u64 *out_bytes_read) override;
    Result WriteFile(FsFile *file, s64 off, const void *buf, u64 write_size, u32 option) override;
    void CloseFile(FsFile *file) override;
    
    // 目录操作
    Result CreateDirectory(const char* path) override;
    Result DeleteDirectoryRecursively(const char* path) override;
    Result RenameDirectory(const char *old_path, const char *new_path) override;
    Result OpenDirectory(const char *path, u32 mode, FsDir *out_dir) override;
    Result ReadDirectory(FsDir *d, s64 *out_total_entries, size_t max_entries, FsDirectoryEntry *buf) override;
    Result GetDirectoryEntryCount(FsDir *d, s64 *out_count) override;
    void CloseDirectory(FsDir *d) override;
    
    // 多线程传输支持
    bool MultiThreadTransfer(s64 size, bool read) override;

private:
    FsFileSystem* m_fs;         // 文件系统指针
    
    // 路径修正函数
    const char* FixPath(const char* path, char* out = nullptr) const;
};

// NX MOD MANAGER文件系统代理类 - 简单实现
class NxModManagerProxy : public haze::FileSystemProxyImpl {
public:
    NxModManagerProxy();
    
    // 基本信息
    const char* GetName() const override;
    const char* GetDisplayName() const override;
    
    // 空间信息
    Result GetTotalSpace(const char *path, s64 *out) override;
    Result GetFreeSpace(const char *path, s64 *out) override;
    Result GetEntryType(const char *path, FsDirEntryType *out_entry_type) override;
    
    // 文件操作
    Result CreateFile(const char* path, s64 size, u32 option) override;
    Result DeleteFile(const char* path) override;
    Result RenameFile(const char *old_path, const char *new_path) override;
    Result OpenFile(const char *path, u32 mode, FsFile *out_file) override;
    Result GetFileSize(FsFile *file, s64 *out_size) override;
    Result SetFileSize(FsFile *file, s64 size) override;
    Result ReadFile(FsFile *file, s64 off, void *buf, u64 read_size, u32 option, u64 *out_bytes_read) override;
    Result WriteFile(FsFile *file, s64 off, const void *buf, u64 write_size, u32 option) override;
    void CloseFile(FsFile *file) override;
    
    // 目录操作
    Result CreateDirectory(const char* path) override;
    Result DeleteDirectoryRecursively(const char* path) override;
    Result RenameDirectory(const char *old_path, const char *new_path) override;
    Result OpenDirectory(const char *path, u32 mode, FsDir *out_dir) override;
    Result ReadDirectory(FsDir *d, s64 *out_total_entries, size_t max_entries, FsDirectoryEntry *buf) override;
    Result GetDirectoryEntryCount(FsDir *d, s64 *out_count) override;
    void CloseDirectory(FsDir *d) override;
    
    // 多线程传输支持
    bool MultiThreadTransfer(s64 size, bool read) override;

private:
    FsFileSystem* m_fs;         // 文件系统指针
    
    // 路径修正函数
    const char* FixPath(const char* path, char* out = nullptr) const;
};

// MTP管理器类
class MtpManager {
public:
    MtpManager();
    ~MtpManager();

    // 禁用拷贝构造和赋值
    MtpManager(const MtpManager&) = delete;
    MtpManager& operator=(const MtpManager&) = delete;

    // 单例模式接口
    static MtpManager& GetInstance();   // 获取单例实例

    // 核心功能接口
    bool StartMtp();                    // 启动MTP服务
    void StopMtp();                     // 强制停止MTP服务
    bool ToggleMtp();                   // 切换MTP状态（关闭则开启，开启则关闭）
    MtpStatus GetStatus() const;        // 获取MTP状态
    std::string GetTransferInfo();      // 获取传输状态信息（格式化字符串）
    void ClearCompletionFlag();         // 清除传输完成标志

    // 状态查询
    bool IsRunning() const;             // 是否正在运行
    bool IsTransferActive() const;      // 是否有传输任务正在进行

private:
    MtpStatus m_status;                                     // 当前状态
    std::string m_transfer_status_text;                     // 传输状态文本信息
    bool m_is_connected;                                    // 是否已连接到电脑
    bool m_just_completed;                                  // 刚刚完成传输的标志（用于UI检测）
    bool m_transfer_in_progress;                            // 传输任务是否正在进行（更准确的判断标志）
    std::shared_ptr<SdCardFileSystemProxy> m_sd_proxy;      // SD卡代理
    std::shared_ptr<AddModProxy> m_addmod_proxy;            // ADD MOD代理
    std::shared_ptr<NxModManagerProxy> m_nxmodmgr_proxy;    // NX MOD MANAGER代理
    haze::FsEntries m_fs_entries;                           // 文件系统入口列表
    mutable std::mutex m_mutex;                             // 互斥锁
    char transfer_filename[256];                             // 当前传输的文件名（用于Progress回调）
    
    // 速度计算相关变量（写入和读取共用）
    u64 m_last_progress_time_ns;                            // 上次进度更新时间（纳秒）
    s64 m_last_progress_offset;                             // 上次进度的偏移量
    double m_current_speed_mbps;                            // 当前传输速度(MB/s)

    // 回调处理
    static void MtpCallback(const haze::CallbackData* data);
    static MtpManager* s_instance;                          // 单例实例指针

    // 内部方法
    void UpdateTransferInfo(const haze::CallbackData* data);
    void ResetTransferInfo();
};

} // namespace mtp