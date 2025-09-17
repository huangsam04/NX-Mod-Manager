// MTP有BUG，很难受，懒得细搞了，瞎逼弄弄拉倒
// MTP传输大点文件容易卡死

#include "mtp_manager.hpp"
#include <cstring>
#include <cstdio>
#include <strings.h>  // 添加strncasecmp函数的头文件

#include "lang_manager.hpp"



namespace mtp {

// 静态成员初始化
MtpManager* MtpManager::s_instance = nullptr;

//=============================================================================
// SdCardFileSystemProxy 实现
//=============================================================================

SdCardFileSystemProxy::SdCardFileSystemProxy() {
    // 获取SD卡文件系统
    m_fs = fsdevGetDeviceFileSystem("sdmc");
}

SdCardFileSystemProxy::~SdCardFileSystemProxy() {
    // SD卡文件系统由系统管理，无需手动释放
}

const char* SdCardFileSystemProxy::GetName() const {
    return "";  // 根目录
}

const char* SdCardFileSystemProxy::GetDisplayName() const {
    return "1.SD Card";  // 显示名称
}

const char* SdCardFileSystemProxy::FixPath(const char* path, char* out) const {
    // 路径修正：参考libhaze示例实现
    static char buf[FS_MAX_PATH];
    const auto len = std::strlen(GetName());

    if (!out) {
        out = buf;
    }

    // 先进行原有的路径处理逻辑
    if (len && !strncasecmp(path + 1, GetName(), len)) {
        std::snprintf(out, FS_MAX_PATH, "/%s", path + 1 + len);
    } else {
        std::strcpy(out, path);
    }

    // 处理中文字符：将非ASCII字符替换为连字符
    char* p = out;
    while (*p) {
        if ((*p & 0x80) != 0) {  // 非ASCII字符
            *p = '-';
        }
        p++;
    }

    return out;
}

Result SdCardFileSystemProxy::GetTotalSpace(const char *path, s64 *out) {
    return fsFsGetTotalSpace(m_fs, FixPath(path), out);
}

Result SdCardFileSystemProxy::GetFreeSpace(const char *path, s64 *out) {
    return fsFsGetFreeSpace(m_fs, FixPath(path), out);
}

Result SdCardFileSystemProxy::GetEntryType(const char *path, FsDirEntryType *out_entry_type) {
    return fsFsGetEntryType(m_fs, FixPath(path), out_entry_type);
}

Result SdCardFileSystemProxy::CreateFile(const char* path, s64 size, u32 option) {
    return fsFsCreateFile(m_fs, FixPath(path), size, option);
}

Result SdCardFileSystemProxy::DeleteFile(const char* path) {
    return fsFsDeleteFile(m_fs, FixPath(path));
}

Result SdCardFileSystemProxy::RenameFile(const char *old_path, const char *new_path) {
    char fixed_old[FS_MAX_PATH], fixed_new[FS_MAX_PATH];
    return fsFsRenameFile(m_fs, FixPath(old_path, fixed_old), FixPath(new_path, fixed_new));
}

Result SdCardFileSystemProxy::OpenFile(const char *path, u32 mode, FsFile *out_file) {
    return fsFsOpenFile(m_fs, FixPath(path), mode, out_file);
}

Result SdCardFileSystemProxy::GetFileSize(FsFile *file, s64 *out_size) {
    return fsFileGetSize(file, out_size);
}

Result SdCardFileSystemProxy::SetFileSize(FsFile *file, s64 size) {
    return fsFileSetSize(file, size);
}

Result SdCardFileSystemProxy::ReadFile(FsFile *file, s64 off, void *buf, u64 read_size, u32 option, u64 *out_bytes_read) {
    return fsFileRead(file, off, buf, read_size, option, out_bytes_read);
}

Result SdCardFileSystemProxy::WriteFile(FsFile *file, s64 off, const void *buf, u64 write_size, u32 option) {
    // 临时注释掉分块写入机制，改为直接写入来测试重复传输大文件是否卡死
    // Temporarily comment out chunked writing mechanism, use direct write to test repeated large file transfer freeze
    
    // 直接写入，不进行分块处理 / Direct write without chunking
    return fsFileWrite(file, off, buf, write_size, option);
    
    
    // // 原分块写入代码 / Original chunked writing code
    // const u64 CHUNK_SIZE = 1*1024 * 1024; // 1MB分块大小，减少Switch Lite的调用次数限制 / 1MB chunk size to reduce call count limit on Switch Lite
    // const u8* data_ptr = static_cast<const u8*>(buf);
    // u64 remaining = write_size;
    // s64 current_offset = off;
    // Result result = 0;
    
    // // 如果写入大小小于分块大小，直接写入
    // // If write size is smaller than chunk size, write directly
    // if (write_size <= CHUNK_SIZE) {
    //     result = fsFileWrite(file, off, buf, write_size, option);
    //     return result;
    // }
    
    // // 分块写入大文件 / Write large file in chunks
    // while (remaining > 0) {
    //     u64 current_chunk_size = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        
    //     // 写入当前分块 / Write current chunk
    //     result = fsFileWrite(file, current_offset, data_ptr, current_chunk_size, option);
    //     if (R_FAILED(result)) {
    //         return result; // 写入失败，返回错误 / Write failed, return error
    //     }
        
    //     // 更新指针和偏移 / Update pointer and offset
    //     data_ptr += current_chunk_size;
    //     current_offset += current_chunk_size;
    //     remaining -= current_chunk_size;
    // }
    
    // return result;
    
}

void SdCardFileSystemProxy::CloseFile(FsFile *file) {
    fsFileClose(file);
}

Result SdCardFileSystemProxy::CreateDirectory(const char* path) {
    return fsFsCreateDirectory(m_fs, FixPath(path));
}

Result SdCardFileSystemProxy::DeleteDirectoryRecursively(const char* path) {
    return fsFsDeleteDirectoryRecursively(m_fs, FixPath(path));
}

Result SdCardFileSystemProxy::RenameDirectory(const char *old_path, const char *new_path) {
    char fixed_old[FS_MAX_PATH], fixed_new[FS_MAX_PATH];
    return fsFsRenameDirectory(m_fs, FixPath(old_path, fixed_old), FixPath(new_path, fixed_new));
}

Result SdCardFileSystemProxy::OpenDirectory(const char *path, u32 mode, FsDir *out_dir) {
    return fsFsOpenDirectory(m_fs, FixPath(path), mode, out_dir);
}

Result SdCardFileSystemProxy::ReadDirectory(FsDir *d, s64 *out_total_entries, size_t max_entries, FsDirectoryEntry *buf) {
    return fsDirRead(d, out_total_entries, max_entries, buf);
}

Result SdCardFileSystemProxy::GetDirectoryEntryCount(FsDir *d, s64 *out_count) {
    return fsDirGetEntryCount(d, out_count);
}

void SdCardFileSystemProxy::CloseDirectory(FsDir *d) {
    fsDirClose(d);
}

bool SdCardFileSystemProxy::MultiThreadTransfer(s64 size, bool read) {
    
    return true;
}

//=============================================================================
// AddModProxy 实现 - 简单版本，参考libhaze示例
//=============================================================================

AddModProxy::AddModProxy() {
    // 获取SD卡文件系统，与SdCardFileSystemProxy相同的方式
    m_fs = fsdevGetDeviceFileSystem("sdmc");
}

const char* AddModProxy::GetName() const {
    return "mods2/0000-add-mod-0000/";  // 挂载路径
}

const char* AddModProxy::GetDisplayName() const {
    return "2.ADD MOD";  // 显示名称
}

const char* AddModProxy::FixPath(const char* path, char* out) const {
    // 路径修正：参考SD卡代理的实现
    static char buf[FS_MAX_PATH];
    const auto len = std::strlen(GetName());

    if (!out) {
        out = buf;
    }

    // 先进行原有的路径处理逻辑
    if (len && !strncasecmp(path + 1, GetName(), len)) {
        std::snprintf(out, FS_MAX_PATH, "/mods2/0000-add-mod-0000%s", path + 1 + len);
    } else {
        std::snprintf(out, FS_MAX_PATH, "/mods2/0000-add-mod-0000%s", path);
    }

    // 处理中文字符：将非ASCII字符替换为连字符
    char* p = out;
    while (*p) {
        if ((*p & 0x80) != 0) {  // 非ASCII字符
            *p = '-';
        }
        p++;
    }

    return out;
}

// 空间信息
Result AddModProxy::GetTotalSpace(const char *path, s64 *out) {
    return fsFsGetTotalSpace(m_fs, FixPath(path), out);
}

Result AddModProxy::GetFreeSpace(const char *path, s64 *out) {
    return fsFsGetFreeSpace(m_fs, FixPath(path), out);
}

Result AddModProxy::GetEntryType(const char *path, FsDirEntryType *out_entry_type) {
    return fsFsGetEntryType(m_fs, FixPath(path), out_entry_type);
}

// 文件操作
Result AddModProxy::CreateFile(const char* path, s64 size, u32 option) {
    // 参考项目策略：创建文件但不预分配大小，避免大文件创建时卡死
    // Reference project strategy: create file without pre-allocation to avoid hanging on large files
    return fsFsCreateFile(m_fs, FixPath(path), size, option);
}

Result AddModProxy::DeleteFile(const char* path) {
    return fsFsDeleteFile(m_fs, FixPath(path));
}

Result AddModProxy::RenameFile(const char *old_path, const char *new_path) {
    char fixed_old[FS_MAX_PATH], fixed_new[FS_MAX_PATH];
    return fsFsRenameFile(m_fs, FixPath(old_path, fixed_old), FixPath(new_path, fixed_new));
}

Result AddModProxy::OpenFile(const char *path, u32 mode, FsFile *out_file) {
    return fsFsOpenFile(m_fs, FixPath(path), mode, out_file);
}

Result AddModProxy::GetFileSize(FsFile *file, s64 *out_size) {
    return fsFileGetSize(file, out_size);
}

Result AddModProxy::SetFileSize(FsFile *file, s64 size) {
    return fsFileSetSize(file, size);
}

Result AddModProxy::ReadFile(FsFile *file, s64 off, void *buf, u64 read_size, u32 option, u64 *out_bytes_read) {
    return fsFileRead(file, off, buf, read_size, option, out_bytes_read);
}

Result AddModProxy::WriteFile(FsFile *file, s64 off, const void *buf, u64 write_size, u32 option) {
    // 使用与SD卡代理相同的分块写入策略，避免大文件写入时卡死
    // Use the same chunked write strategy as SD card proxy to avoid hanging on large files
    // const u64 CHUNK_SIZE = 1024 * 1024; // 1MB 分块大小 / 1MB chunk size
    
    // Result result = 0;
    // u64 remaining = write_size;
    // s64 current_offset = off;
    // const u8* data_ptr = static_cast<const u8*>(buf);
    
    // // 分块写入大文件 / Write large file in chunks
    // while (remaining > 0) {
    //     u64 current_chunk_size = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        
    //     // 写入当前分块 / Write current chunk
    //     result = fsFileWrite(file, current_offset, data_ptr, current_chunk_size, option);
    //     if (R_FAILED(result)) {
    //         return result; // 写入失败，返回错误 / Write failed, return error
    //     }
        
    //     // 更新指针和偏移 / Update pointer and offset
    //     data_ptr += current_chunk_size;
    //     current_offset += current_chunk_size;
    //     remaining -= current_chunk_size;
    // }
    
    // return result;

    // 直接写入，不进行分块处理 / Direct write without chunking
    return fsFileWrite(file, off, buf, write_size, option);
}

void AddModProxy::CloseFile(FsFile *file) {
    fsFileClose(file);
}

// 目录操作
Result AddModProxy::CreateDirectory(const char* path) {
    return fsFsCreateDirectory(m_fs, FixPath(path));
}

Result AddModProxy::DeleteDirectoryRecursively(const char* path) {
    return fsFsDeleteDirectoryRecursively(m_fs, FixPath(path));
}

Result AddModProxy::RenameDirectory(const char *old_path, const char *new_path) {
    char fixed_old[FS_MAX_PATH], fixed_new[FS_MAX_PATH];
    return fsFsRenameDirectory(m_fs, FixPath(old_path, fixed_old), FixPath(new_path, fixed_new));
}

Result AddModProxy::OpenDirectory(const char *path, u32 mode, FsDir *out_dir) {
    return fsFsOpenDirectory(m_fs, FixPath(path), mode, out_dir);
}

Result AddModProxy::ReadDirectory(FsDir *d, s64 *out_total_entries, size_t max_entries, FsDirectoryEntry *buf) {
    return fsDirRead(d, out_total_entries, max_entries, buf);
}

Result AddModProxy::GetDirectoryEntryCount(FsDir *d, s64 *out_count) {
    return fsDirGetEntryCount(d, out_count);
}

void AddModProxy::CloseDirectory(FsDir *d) {
    fsDirClose(d);
}

bool AddModProxy::MultiThreadTransfer(s64 size, bool read) {
    return true;
}

//=============================================================================
// NxModManagerProxy 实现
//=============================================================================

NxModManagerProxy::NxModManagerProxy() {
    // 获取SD卡文件系统，与SdCardFileSystemProxy相同的方式
    m_fs = fsdevGetDeviceFileSystem("sdmc");
}

const char* NxModManagerProxy::GetName() const {
    return "mods2/";
}

const char* NxModManagerProxy::GetDisplayName() const {
    return "3.Nx Mod Manager";
}

const char* NxModManagerProxy::FixPath(const char* path, char* out) const {
    // 路径修正：参考SD卡代理的实现
    static char buf[FS_MAX_PATH];
    const auto len = std::strlen(GetName());

    if (!out) {
        out = buf;
    }

    // 先进行原有的路径处理逻辑
    if (len && !strncasecmp(path + 1, GetName(), len)) {
        std::snprintf(out, FS_MAX_PATH, "/mods2%s", path + 1 + len);
    } else {
        std::snprintf(out, FS_MAX_PATH, "/mods2%s", path);
    }

    // 处理中文字符：将非ASCII字符替换为连字符
    char* p = out;
    while (*p) {
        if ((*p & 0x80) != 0) {  // 非ASCII字符
            *p = '-';
        }
        p++;
    }

    return out;
}

// 空间信息
Result NxModManagerProxy::GetTotalSpace(const char *path, s64 *out) {
    return fsFsGetTotalSpace(m_fs, FixPath(path), out);
}

Result NxModManagerProxy::GetFreeSpace(const char *path, s64 *out) {
    return fsFsGetFreeSpace(m_fs, FixPath(path), out);
}

Result NxModManagerProxy::GetEntryType(const char *path, FsDirEntryType *out_entry_type) {
    return fsFsGetEntryType(m_fs, FixPath(path), out_entry_type);
}

// 文件操作
Result NxModManagerProxy::CreateFile(const char* path, s64 size, u32 option) {
    // 参考项目策略：创建文件但不预分配大小，避免大文件创建时卡死
    // Reference project strategy: create file without pre-allocation to avoid hanging on large files
    return fsFsCreateFile(m_fs, FixPath(path), size, option);
}

Result NxModManagerProxy::DeleteFile(const char* path) {
    return fsFsDeleteFile(m_fs, FixPath(path));
}

Result NxModManagerProxy::RenameFile(const char *old_path, const char *new_path) {
    char fixed_old[FS_MAX_PATH], fixed_new[FS_MAX_PATH];
    return fsFsRenameFile(m_fs, FixPath(old_path, fixed_old), FixPath(new_path, fixed_new));
}

Result NxModManagerProxy::OpenFile(const char *path, u32 mode, FsFile *out_file) {
    return fsFsOpenFile(m_fs, FixPath(path), mode, out_file);
}

Result NxModManagerProxy::GetFileSize(FsFile *file, s64 *out_size) {
    return fsFileGetSize(file, out_size);
}

Result NxModManagerProxy::SetFileSize(FsFile *file, s64 size) {
    return fsFileSetSize(file, size);
}

Result NxModManagerProxy::ReadFile(FsFile *file, s64 off, void *buf, u64 read_size, u32 option, u64 *out_bytes_read) {
    return fsFileRead(file, off, buf, read_size, option, out_bytes_read);
}

Result NxModManagerProxy::WriteFile(FsFile *file, s64 off, const void *buf, u64 write_size, u32 option) {
    // 使用与SD卡代理相同的分块写入策略，避免大文件写入时卡死
    // Use the same chunked write strategy as SD card proxy to avoid hanging on large files
    // const u64 CHUNK_SIZE = 1024 * 1024; // 1MB 分块大小 / 1MB chunk size
    
    // Result result = 0;
    // u64 remaining = write_size;
    // s64 current_offset = off;
    // const u8* data_ptr = static_cast<const u8*>(buf);
    
    // // 分块写入大文件 / Write large file in chunks
    // while (remaining > 0) {
    //     u64 current_chunk_size = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        
    //     // 写入当前分块 / Write current chunk
    //     result = fsFileWrite(file, current_offset, data_ptr, current_chunk_size, option);
    //     if (R_FAILED(result)) {
    //         return result; // 写入失败，返回错误 / Write failed, return error
    //     }
        
    //     // 更新指针和偏移 / Update pointer and offset
    //     data_ptr += current_chunk_size;
    //     current_offset += current_chunk_size;
    //     remaining -= current_chunk_size;
    // }
    
    // return result;

    // 直接写入，不进行分块处理 / Direct write without chunking
    return fsFileWrite(file, off, buf, write_size, option);
}

void NxModManagerProxy::CloseFile(FsFile *file) {
    fsFileClose(file);
}

// 目录操作
Result NxModManagerProxy::CreateDirectory(const char* path) {
    return fsFsCreateDirectory(m_fs, FixPath(path));
}

Result NxModManagerProxy::DeleteDirectoryRecursively(const char* path) {
    return fsFsDeleteDirectoryRecursively(m_fs, FixPath(path));
}

Result NxModManagerProxy::RenameDirectory(const char *old_path, const char *new_path) {
    char fixed_old[FS_MAX_PATH], fixed_new[FS_MAX_PATH];
    return fsFsRenameDirectory(m_fs, FixPath(old_path, fixed_old), FixPath(new_path, fixed_new));
}

Result NxModManagerProxy::OpenDirectory(const char *path, u32 mode, FsDir *out_dir) {
    return fsFsOpenDirectory(m_fs, FixPath(path), mode, out_dir);
}

Result NxModManagerProxy::ReadDirectory(FsDir *d, s64 *out_total_entries, size_t max_entries, FsDirectoryEntry *buf) {
    return fsDirRead(d, out_total_entries, max_entries, buf);
}

Result NxModManagerProxy::GetDirectoryEntryCount(FsDir *d, s64 *out_count) {
    return fsDirGetEntryCount(d, out_count);
}

void NxModManagerProxy::CloseDirectory(FsDir *d) {
    fsDirClose(d);
}

bool NxModManagerProxy::MultiThreadTransfer(s64 size, bool read) {
    return true;
}

//=============================================================================
// MtpManager 实现
//=============================================================================

MtpManager& MtpManager::GetInstance() {
    // 使用静态局部变量实现线程安全的单例模式 (Thread-safe singleton using static local variable)
    static MtpManager instance;
    return instance;
}

MtpManager::MtpManager() 
    : m_status(MtpStatus::Stopped)
    , m_transfer_status_text("")
    , m_is_connected(false)
    , m_just_completed(false)
    , m_transfer_in_progress(false)
    , m_sd_proxy(nullptr)
    , m_addmod_proxy(nullptr)
    , m_nxmodmgr_proxy(nullptr)
    

{
    // 设置单例实例
    s_instance = this;
    
    // 初始化传输信息
    ResetTransferInfo();
    
    // 创建SD卡代理
    m_sd_proxy = std::make_shared<SdCardFileSystemProxy>();
    
    // 创建ADD MOD代理
    m_addmod_proxy = std::make_shared<AddModProxy>();
    
    // 创建Nx Mod Manager代理
    m_nxmodmgr_proxy = std::make_shared<NxModManagerProxy>();
    
    // 添加到文件系统入口列表
    m_fs_entries.push_back(m_sd_proxy);
    m_fs_entries.push_back(m_addmod_proxy);
    m_fs_entries.push_back(m_nxmodmgr_proxy);
}

MtpManager::~MtpManager() {
    // 确保MTP服务已停止
    if (IsRunning()) {
        StopMtp();
    }
    
    // 清理单例实例
    s_instance = nullptr;
}

bool MtpManager::StartMtp() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 检查当前状态
    if (m_status != MtpStatus::Stopped) {
        return false;  // 已经在运行或正在启动/停止
    }
    
    // 设置启动状态
    m_status = MtpStatus::Starting;
    
    // 重置传输信息
    ResetTransferInfo();
    
    // 初始化haze MTP服务
    // 参数：回调函数、文件系统入口、VID、PID标识、日志关闭
    bool result = haze::Initialize(MtpCallback, m_fs_entries, 0x057e, 0x201d, false);
    
    if (result) {
        m_status = MtpStatus::Running;
    } else {
        m_status = MtpStatus::Stopped;
    }
    
    return result;
}

void MtpManager::StopMtp() {
    // 先获取锁，清理所有状态，然后释放锁再调用haze::Exit()避免死锁
    // 参考sphaira项目的实现方式
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 重置传输信息
        ResetTransferInfo();
        
        // 设置为已停止状态
        m_status = MtpStatus::Stopped;
        
        // 重置连接状态
        m_is_connected = false;
        m_just_completed = false;
        
        // 清空传输状态文本
        m_transfer_status_text.clear();
    }
    
    // 在没有锁的情况下调用haze::Exit()，避免死锁
    haze::Exit();
}



bool MtpManager::ToggleMtp() {
    // 获取当前状态（不需要锁，因为GetStatus内部已经有锁）
    MtpStatus current_status = GetStatus();
    
    // 根据当前状态进行切换
    if (current_status == MtpStatus::Stopped) {
        // 如果MTP已停止，则启动MTP
        StartMtp();
        return true;
    } else if (current_status == MtpStatus::Running) {
        // 如果MTP正在运行且有传输任务正在进行，返回false让用户确认
        if (IsTransferActive()) {
            return false;  // 提示用户确认是否关闭
        }
        // 如果MTP正在运行，则停止MTP
        StopMtp();
        return true;  // StopMtp没有返回值，假设总是成功
    }
}

MtpStatus MtpManager::GetStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

std::string MtpManager::GetTransferInfo() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 根据当前MTP状态和连接状态返回相应的状态文本
    switch (m_status) {
        case MtpStatus::Stopped:
            return MTP_NOT_RUNNING_TEXT;
            
        case MtpStatus::Running:
            if (!m_is_connected) {
                return MTP_NOUSB_RUNNING_TEXT;
            } 
            return m_transfer_status_text;
    }
}

bool MtpManager::IsRunning() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status == MtpStatus::Running;
}

bool MtpManager::IsTransferActive() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // 使用专门的传输进度标志来判断，比检查文本更准确
    return m_transfer_in_progress;
}



void MtpManager::MtpCallback(const haze::CallbackData* data) {
    // 确保实例存在
    if (s_instance) {
        s_instance->UpdateTransferInfo(data);
    }
}

void MtpManager::UpdateTransferInfo(const haze::CallbackData* data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!data) return;
    
    switch (data->type) {
        case haze::CallbackType_ReadBegin:
            // 开始读取文件 - 直接生成格式化状态文本，只显示文件名（不含路径）
            {
                const char* filename = data->file.filename;
                const char* basename = strrchr(filename, '/');
                if (basename) {
                    basename++; // 跳过 '/' 字符
                } else {
                    basename = filename; // 没有路径分隔符，直接使用原文件名
                }
                strncpy(transfer_filename, basename, sizeof(transfer_filename) - 1);
                transfer_filename[sizeof(transfer_filename) - 1] = '\0'; // 确保字符串结束
                
                // 初始化速度计算变量
                m_last_progress_time_ns = armGetSystemTick();
                m_last_progress_offset = 0;
                m_current_speed_mbps = 0.0;
            }
            m_just_completed = false;
            m_transfer_in_progress = true;  // 设置传输进行标志
            // svcSleepThread(1000000);
            break;
            
        case haze::CallbackType_WriteBegin:
            // 开始写入文件 - 直接生成格式化状态文本，只显示文件名（不含路径）
            {
                const char* filename = data->file.filename;
                const char* basename = strrchr(filename, '/');
                if (basename) {
                    basename++; // 跳过 '/' 字符
                } else {
                    basename = filename; // 没有路径分隔符，直接使用原文件名
                }
                strncpy(transfer_filename, basename, sizeof(transfer_filename) - 1);
                transfer_filename[sizeof(transfer_filename) - 1] = '\0'; // 确保字符串结束
                
                // 初始化速度计算变量
                m_last_progress_time_ns = armGetSystemTick();
                m_last_progress_offset = 0;
                m_current_speed_mbps = 0.0;
            }
            
            m_just_completed = false;
            m_transfer_in_progress = true;  // 设置传输进行标志
            // svcSleepThread(1000000);
            break;
            
        case haze::CallbackType_ReadProgress:
            // 读取进度更新 - 显示速度和已读取大小
            {
                if (data->progress.offset > 0) {
                    // 获取当前时间
                    u64 current_time_ns = armGetSystemTick();
                    
                    // 计算时间差（转换为秒）
                    double time_diff_seconds = (double)(current_time_ns - m_last_progress_time_ns) / 19200000.0;
                    
                    // 如果时间间隔大于0.5秒，更新速度计算
                    if (time_diff_seconds >= 0.5) {
                        // 计算数据量差（字节）
                        s64 data_diff_bytes = data->progress.offset - m_last_progress_offset;
                        
                        // 计算速度（MB/s）
                        if (time_diff_seconds > 0) {
                            m_current_speed_mbps = (double)data_diff_bytes / (1024.0 * 1024.0) / time_diff_seconds;
                        }
                        
                        // 更新记录的时间和偏移量
                        m_last_progress_time_ns = current_time_ns;
                        m_last_progress_offset = data->progress.offset;
                    }
                    
                    // 计算已读取大小
                    double progress_mb = (double)data->progress.offset / (1024.0 * 1024.0);
                    
                    char progress_text[256];
                    snprintf(progress_text, sizeof(progress_text), 
                        MTP_READING_PROGRESS_TAG.c_str(), 
                        progress_mb, m_current_speed_mbps, transfer_filename);
                    m_transfer_status_text = std::string(progress_text);
                }
            }
            m_transfer_in_progress = true;  // 设置传输进行标志
            break;
            
        case haze::CallbackType_WriteProgress:
            // 写入进度更新 - 显示速度和已写入大小
            {
                if (data->progress.offset > 0) {
                    // 获取当前时间
                    u64 current_time_ns = armGetSystemTick();
                    
                    // 计算时间差（转换为秒）
                    double time_diff_seconds = (double)(current_time_ns - m_last_progress_time_ns) / 19200000.0;
                    
                    // 如果时间间隔大于0.5秒，更新速度计算
                    if (time_diff_seconds >= 0.5) {
                        // 计算数据量差（字节）
                        s64 data_diff_bytes = data->progress.offset - m_last_progress_offset;
                        
                        // 计算速度（MB/s）
                        if (time_diff_seconds > 0) {
                            m_current_speed_mbps = (double)data_diff_bytes / (1024.0 * 1024.0) / time_diff_seconds;
                        }
                        
                        // 更新记录的时间和偏移量
                        m_last_progress_time_ns = current_time_ns;
                        m_last_progress_offset = data->progress.offset;
                    }
                    
                    // 计算已写入大小
                    double progress_mb = (double)data->progress.offset / (1024.0 * 1024.0);
                    
                    char progress_text[256];
                    snprintf(progress_text, sizeof(progress_text), 
                        MTP_WRITEING_PROGRESS_TAG.c_str(), 
                        progress_mb, m_current_speed_mbps, transfer_filename);
                    m_transfer_status_text = std::string(progress_text);
                }
            }
            m_transfer_in_progress = true;  // 设置传输进行标志
            break;
            
        case haze::CallbackType_ReadEnd:
            // 读取传输结束，只显示文件名（不含路径）
            {
                std::string filename = std::string(data->file.filename);
                size_t pos = filename.find_last_of('/');
                if (pos != std::string::npos) {
                    filename = filename.substr(pos + 1);
                }
                m_transfer_status_text = MTP_READING_DONE_TAG + filename;
            }
            m_just_completed = true;  // 设置刚完成标志，供UI检测
            m_transfer_in_progress = false;  // 清除传输进行标志
            // svcSleepThread(1000000); // 延迟50毫秒，让UI有时间显示
            break;
            
        case haze::CallbackType_WriteEnd:
            // 写入传输结束，只显示文件名（不含路径）
            {
                std::string filename = std::string(data->file.filename);
                size_t pos = filename.find_last_of('/');
                if (pos != std::string::npos) {
                    filename = filename.substr(pos + 1);
                }
                m_transfer_status_text = MTP_WRITEING_DONE_TAG + filename;
            }
            m_just_completed = true;  // 设置刚完成标志，供UI检测
            m_transfer_in_progress = false;  // 清除传输进行标志
            // svcSleepThread(1000000); // 延迟50毫秒，让UI有时间显示
            break;
            
        case haze::CallbackType_OpenSession:
            // MTP会话已打开，设置连接状态
            m_is_connected = true;
            if (m_transfer_status_text.empty()) {
                m_transfer_status_text = MTP_STATUS_CONNECTED_TEXT;
            }
            // svcSleepThread(1000000); // 延迟30毫秒，让UI有时间显示
            break;
            
        case haze::CallbackType_CloseSession:
            // USB连接断开时的处理 (Handle USB disconnection)
            m_is_connected = false;
            m_transfer_status_text = MTP_STATUS_DISCONNECTED_TEXT;
            // svcSleepThread(1000000); // 延迟50毫秒，让UI有时间显示
            break;
            
        case haze::CallbackType_CreateFile:
            
                
            m_transfer_status_text = MTP_CREATE_FILE_TAG + std::string(data->file.filename);
            
            // svcSleepThread(2000000);
            break;
            
        case haze::CallbackType_DeleteFile:
            // 删除文件操作，只显示文件名（不含路径）
            {
                std::string filename = std::string(data->file.filename);
                size_t pos = filename.find_last_of('/');
                if (pos != std::string::npos) {
                    filename = filename.substr(pos + 1);
                }
                m_transfer_status_text = MTP_DELETE_FILE_TAG + filename;
            }
            // svcSleepThread(1000000); // 延迟50毫秒，让UI有时间显示
            break;
            
        case haze::CallbackType_RenameFile:
            // 重命名文件操作，只显示文件名（不含路径）
            {
                std::string old_filename = std::string(data->rename.filename);
                std::string new_filename = std::string(data->rename.newname);
                
                size_t pos = old_filename.find_last_of('/');
                if (pos != std::string::npos) {
                    old_filename = old_filename.substr(pos + 1);
                }
                
                pos = new_filename.find_last_of('/');
                if (pos != std::string::npos) {
                    new_filename = new_filename.substr(pos + 1);
                }
                
                m_transfer_status_text = MTP_RENAME_FILE_TAG + old_filename + " -> " + new_filename;
            }
            // svcSleepThread(1000000); // 延迟50毫秒，让UI有时间显示
            break;
            
        case haze::CallbackType_CreateFolder:
            // 创建文件夹操作，只显示文件夹名（不含路径）
            {
                std::string foldername = std::string(data->file.filename);
                size_t pos = foldername.find_last_of('/');
                if (pos != std::string::npos) {
                    foldername = foldername.substr(pos + 1);
                }
                m_transfer_status_text = MTP_CREATE_FOLDER_TAG + foldername;
            }
            // svcSleepThread(1000000); // 延迟50毫秒，让UI有时间显示
            break;
            
        case haze::CallbackType_DeleteFolder:
            // 删除文件夹操作，只显示文件夹名（不含路径）
            {
                std::string foldername = std::string(data->file.filename);
                size_t pos = foldername.find_last_of('/');
                if (pos != std::string::npos) {
                    foldername = foldername.substr(pos + 1);
                }
                m_transfer_status_text = MTP_DELETE_FOLDER_TAG + foldername;
            }
            // svcSleepThread(1000000); // 延迟50毫秒，让UI有时间显示
            break;
            
        case haze::CallbackType_RenameFolder:
            // 重命名文件夹操作，只显示文件夹名（不含路径）
            {
                std::string old_foldername = std::string(data->rename.filename);
                std::string new_foldername = std::string(data->rename.newname);
                
                size_t pos = old_foldername.find_last_of('/');
                if (pos != std::string::npos) {
                    old_foldername = old_foldername.substr(pos + 1);
                }
                
                pos = new_foldername.find_last_of('/');
                if (pos != std::string::npos) {
                    new_foldername = new_foldername.substr(pos + 1);
                }
                
                m_transfer_status_text = MTP_RENAME_FOLDER_TAG + old_foldername + " -> " + new_foldername;
            }
            // svcSleepThread(1000000); // 延迟50毫秒，让UI有时间显示
            break;
    }
}

void MtpManager::ResetTransferInfo() {
    // 重置传输信息（不加锁，由调用者保证线程安全）
    m_transfer_status_text.clear();     // 清空状态文本
    m_is_connected = false;             // 重置连接状态
    m_just_completed = false;           // 重置完成标志
    m_transfer_in_progress = false;     // 重置传输进行标志
}

// 清除传输完成标志
void MtpManager::ClearCompletionFlag() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_just_completed = false;
    // 清除传输完成后的详细信息，重置为基本连接状态
    if (m_is_connected) {
        // m_transfer_status_text = "[MTP状态]：等待传输操作";
    } else {
        m_transfer_status_text.clear();
    }
}



} // namespace mtp