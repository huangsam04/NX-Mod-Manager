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

Result SdCardFileSystemProxy::GetEntryType(const char *path, haze::FileAttrType *out_entry_type) {
    FsDirEntryType type;
    Result rc = fsFsGetEntryType(m_fs, FixPath(path), &type);
    if (R_SUCCEEDED(rc)) {
        *out_entry_type = (type == FsDirEntryType_Dir) ? haze::FileAttrType_DIR : haze::FileAttrType_FILE;
    }
    return rc;
}

Result SdCardFileSystemProxy::GetEntryAttributes(const char *path, haze::FileAttr *out) {
    // 获取文件类型
    FsDirEntryType type;
    Result rc = fsFsGetEntryType(m_fs, FixPath(path), &type);
    if (R_FAILED(rc)) {
        return rc;
    }

    if (type == FsDirEntryType_File) {
        out->type = haze::FileAttrType_FILE;

        // 获取时间戳（如果失败也不影响）
        FsTimeStampRaw timestamp{};
        if (R_SUCCEEDED(fsFsGetFileTimeStampRaw(m_fs, FixPath(path), &timestamp)) && timestamp.is_valid) {
            out->ctime = timestamp.created;
            out->mtime = timestamp.modified;
        }

        // 获取文件大小
        FsFile file;
        rc = fsFsOpenFile(m_fs, FixPath(path), FsOpenMode_Read, &file);
        if (R_SUCCEEDED(rc)) {
            s64 size;
            if (R_SUCCEEDED(fsFileGetSize(&file, &size))) {
                out->size = size;
            }
            fsFileClose(&file);
        }
    } else {
        out->type = haze::FileAttrType_DIR;
    }

    // 设置只读标志（如果需要）
    out->flag = 0;  // 默认可读写

    return 0;  // 成功
}

Result SdCardFileSystemProxy::CreateFile(const char* path, s64 size) {
    u32 flags = 0;
    const s64 _4_GB = 0x100000000;
    if (size >= _4_GB) {
        flags = FsCreateOption_BigFile;
    }

    // 参考libhaze示例：不在这里设置大小，避免长时间阻塞导致超时
    // SEE: https://github.com/ITotalJustice/libhaze/issues/1#issuecomment-3305067733
    return fsFsCreateFile(m_fs, FixPath(path), 0, flags);
}

Result SdCardFileSystemProxy::DeleteFile(const char* path) {
    return fsFsDeleteFile(m_fs, FixPath(path));
}

Result SdCardFileSystemProxy::RenameFile(const char *old_path, const char *new_path) {
    char fixed_old[FS_MAX_PATH], fixed_new[FS_MAX_PATH];
    return fsFsRenameFile(m_fs, FixPath(old_path, fixed_old), FixPath(new_path, fixed_new));
}

Result SdCardFileSystemProxy::OpenFile(const char *path, haze::FileOpenMode mode, haze::File *out_file) {
    u32 flags = FsOpenMode_Read;
    if (mode == haze::FileOpenMode_WRITE) {
        flags = FsOpenMode_Write | FsOpenMode_Append;
    }

    auto f = new FsFile();
    const auto rc = fsFsOpenFile(m_fs, FixPath(path), flags, f);
    if (R_FAILED(rc)) {
        delete f;
        return rc;
    }

    out_file->impl = f;
    return 0;  // 成功
}

Result SdCardFileSystemProxy::GetFileSize(haze::File *file, s64 *out_size) {
    auto f = static_cast<FsFile*>(file->impl);
    return fsFileGetSize(f, out_size);
}

Result SdCardFileSystemProxy::SetFileSize(haze::File *file, s64 size) {
    // 参考libhaze示例：设置为0如果Switch在分配大文件时冻结
    // 这通常发生在使用emuMMC + Windows时
    #if 0
    auto f = static_cast<FsFile*>(file->impl);
    return fsFileSetSize(f, size);
    #else
    return 0;  // 成功，但不实际设置大小
    #endif
}

Result SdCardFileSystemProxy::ReadFile(haze::File *file, s64 off, void *buf, u64 read_size, u64 *out_bytes_read) {
    auto f = static_cast<FsFile*>(file->impl);
    return fsFileRead(f, off, buf, read_size, FsReadOption_None, out_bytes_read);
}

Result SdCardFileSystemProxy::WriteFile(haze::File *file, s64 off, const void *buf, u64 write_size) {
    auto f = static_cast<FsFile*>(file->impl);
    return fsFileWrite(f, off, buf, write_size, FsWriteOption_None);
}

void SdCardFileSystemProxy::CloseFile(haze::File *file) {
    auto f = static_cast<FsFile*>(file->impl);
    if (f) {
        fsFileClose(f);
        delete f;
        file->impl = nullptr;
    }
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

Result SdCardFileSystemProxy::OpenDirectory(const char *path, haze::Dir *out_dir) {
    auto dir = new FsDir();
    const auto rc = fsFsOpenDirectory(m_fs, FixPath(path), FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles | FsDirOpenMode_NoFileSize, dir);
    if (R_FAILED(rc)) {
        delete dir;
        return rc;
    }

    out_dir->impl = dir;
    return 0;  // 成功
}

Result SdCardFileSystemProxy::ReadDirectory(haze::Dir *d, s64 *out_total_entries, size_t max_entries, haze::DirEntry *buf) {
    auto dir = static_cast<FsDir*>(d->impl);

    std::vector<FsDirectoryEntry> entries(max_entries);
    Result rc = fsDirRead(dir, out_total_entries, entries.size(), entries.data());
    if (R_SUCCEEDED(rc)) {
        for (s64 i = 0; i < *out_total_entries; i++) {
            std::strcpy(buf[i].name, entries[i].name);
        }
    }

    return rc;
}

Result SdCardFileSystemProxy::GetDirectoryEntryCount(haze::Dir *d, s64 *out_count) {
    auto dir = static_cast<FsDir*>(d->impl);
    return fsDirGetEntryCount(dir, out_count);
}

void SdCardFileSystemProxy::CloseDirectory(haze::Dir *d) {
    auto dir = static_cast<FsDir*>(d->impl);
    if (dir) {
        fsDirClose(dir);
        delete dir;
        d->impl = nullptr;
    }
}

//=============================================================================
// AddModProxy 实现 - 简单版本，参考libhaze示例
//=============================================================================

AddModProxy::AddModProxy() {
    // 获取SD卡文件系统，与SdCardFileSystemProxy相同的方式
    m_fs = fsdevGetDeviceFileSystem("sdmc");
}

const char* AddModProxy::GetName() const {
    return "add-mod:/";  // 挂载路径标识
}

const char* AddModProxy::GetDisplayName() const {
    return "2.ADD MOD";  // 显示名称
}

const char* AddModProxy::FixPath(const char* path, char* out) const {
    // 路径修正：参考libhaze示例实现
    static char buf[FS_MAX_PATH];
    const auto len = std::strlen(GetName());

    if (!out) {
        out = buf;
    }

    // 按照libhaze示例的逻辑处理路径
    if (len && !strncasecmp(path, GetName(), len)) {
        std::snprintf(out, FS_MAX_PATH, "/mods2/0000-add-mod-0000/%s", path + len);
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

Result AddModProxy::GetEntryType(const char *path, haze::FileAttrType *out_entry_type) {
    FsDirEntryType type;
    Result rc = fsFsGetEntryType(m_fs, FixPath(path), &type);
    if (R_SUCCEEDED(rc)) {
        *out_entry_type = (type == FsDirEntryType_Dir) ? haze::FileAttrType_DIR : haze::FileAttrType_FILE;
    }
    return rc;
}

Result AddModProxy::GetEntryAttributes(const char *path, haze::FileAttr *out) {
    // 获取文件类型
    FsDirEntryType type;
    Result rc = fsFsGetEntryType(m_fs, FixPath(path), &type);
    if (R_FAILED(rc)) {
        return rc;
    }

    if (type == FsDirEntryType_File) {
        out->type = haze::FileAttrType_FILE;

        // 获取时间戳（如果失败也不影响）
        FsTimeStampRaw timestamp{};
        if (R_SUCCEEDED(fsFsGetFileTimeStampRaw(m_fs, FixPath(path), &timestamp)) && timestamp.is_valid) {
            out->ctime = timestamp.created;
            out->mtime = timestamp.modified;
        }

        // 获取文件大小
        FsFile file;
        rc = fsFsOpenFile(m_fs, FixPath(path), FsOpenMode_Read, &file);
        if (R_SUCCEEDED(rc)) {
            s64 size;
            if (R_SUCCEEDED(fsFileGetSize(&file, &size))) {
                out->size = size;
            }
            fsFileClose(&file);
        }
    } else {
        out->type = haze::FileAttrType_DIR;
    }

    // 设置只读标志（如果需要）
    out->flag = 0;  // 默认可读写

    return 0;  // 成功
}

// 文件操作
Result AddModProxy::CreateFile(const char* path, s64 size) {
    u32 flags = 0;
    const s64 _4_GB = 0x100000000;
    if (size >= _4_GB) {
        flags = FsCreateOption_BigFile;
    }

    // 参考libhaze示例：不在这里设置大小，避免长时间阻塞导致超时
    // SEE: https://github.com/ITotalJustice/libhaze/issues/1#issuecomment-3305067733
    return fsFsCreateFile(m_fs, FixPath(path), 0, flags);
}

Result AddModProxy::DeleteFile(const char* path) {
    return fsFsDeleteFile(m_fs, FixPath(path));
}

Result AddModProxy::RenameFile(const char *old_path, const char *new_path) {
    char fixed_old[FS_MAX_PATH], fixed_new[FS_MAX_PATH];
    return fsFsRenameFile(m_fs, FixPath(old_path, fixed_old), FixPath(new_path, fixed_new));
}

Result AddModProxy::OpenFile(const char *path, haze::FileOpenMode mode, haze::File *out_file) {
    // 转换haze::FileOpenMode到FsOpenMode
    u32 fs_mode = FsOpenMode_Read;
    if (mode == haze::FileOpenMode_WRITE) {
        fs_mode = FsOpenMode_Write | FsOpenMode_Append;
    }

    auto file = new FsFile();
    const auto rc = fsFsOpenFile(m_fs, FixPath(path), fs_mode, file);
    if (R_FAILED(rc)) {
        delete file;
        return rc;
    }

    out_file->impl = file;
    return 0;  // 成功
}

Result AddModProxy::GetFileSize(haze::File *file, s64 *out_size) {
    auto f = static_cast<FsFile*>(file->impl);
    return fsFileGetSize(f, out_size);
}

Result AddModProxy::SetFileSize(haze::File *file, s64 size) {
    // 参考libhaze示例：设置为0如果Switch在分配大文件时冻结
    // 这通常发生在使用emuMMC + Windows时
    #if 0
    auto f = static_cast<FsFile*>(file->impl);
    return fsFileSetSize(f, size);
    #else
    return 0;  // 成功，但不实际设置大小
    #endif
}

Result AddModProxy::ReadFile(haze::File *file, s64 off, void *buf, u64 read_size, u64 *out_bytes_read) {
    auto f = static_cast<FsFile*>(file->impl);
    return fsFileRead(f, off, buf, read_size, FsReadOption_None, out_bytes_read);
}

Result AddModProxy::WriteFile(haze::File *file, s64 off, const void *buf, u64 write_size) {
    auto f = static_cast<FsFile*>(file->impl);
    return fsFileWrite(f, off, buf, write_size, FsWriteOption_None);
}

void AddModProxy::CloseFile(haze::File *file) {
    auto f = static_cast<FsFile*>(file->impl);
    if (f) {
        fsFileClose(f);
        delete f;
        file->impl = nullptr;
    }
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

Result AddModProxy::OpenDirectory(const char *path, haze::Dir *out_dir) {
    auto dir = new FsDir();
    const auto rc = fsFsOpenDirectory(m_fs, FixPath(path), FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles | FsDirOpenMode_NoFileSize, dir);
    if (R_FAILED(rc)) {
        delete dir;
        return rc;
    }

    out_dir->impl = dir;
    return 0;  // 成功
}

Result AddModProxy::ReadDirectory(haze::Dir *d, s64 *out_total_entries, size_t max_entries, haze::DirEntry *buf) {
    auto dir = static_cast<FsDir*>(d->impl);

    std::vector<FsDirectoryEntry> entries(max_entries);
    Result rc = fsDirRead(dir, out_total_entries, entries.size(), entries.data());
    if (R_SUCCEEDED(rc)) {
        for (s64 i = 0; i < *out_total_entries; i++) {
            std::strcpy(buf[i].name, entries[i].name);
        }
    }

    return rc;
}

Result AddModProxy::GetDirectoryEntryCount(haze::Dir *d, s64 *out_count) {
    auto dir = static_cast<FsDir*>(d->impl);
    return fsDirGetEntryCount(dir, out_count);
}

void AddModProxy::CloseDirectory(haze::Dir *d) {
    auto dir = static_cast<FsDir*>(d->impl);
    if (dir) {
        fsDirClose(dir);
        delete dir;
        d->impl = nullptr;
    }
}

//=============================================================================
// NxModManagerProxy 实现
//=============================================================================

NxModManagerProxy::NxModManagerProxy() {
    // 获取SD卡文件系统，与SdCardFileSystemProxy相同的方式
    m_fs = fsdevGetDeviceFileSystem("sdmc");
}

const char* NxModManagerProxy::GetName() const {
    return "nx-mod:/";
}

const char* NxModManagerProxy::GetDisplayName() const {
    return "3.Nx Mod Manager";
}

const char* NxModManagerProxy::FixPath(const char* path, char* out) const {
    // 路径修正：参考libhaze示例实现
    static char buf[FS_MAX_PATH];
    const auto len = std::strlen(GetName());

    if (!out) {
        out = buf;
    }

    // 按照libhaze示例的逻辑处理路径
    if (len && !strncasecmp(path, GetName(), len)) {
        std::snprintf(out, FS_MAX_PATH, "/mods2/%s", path + len);
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

Result NxModManagerProxy::GetEntryType(const char *path, haze::FileAttrType *out_entry_type) {
    FsDirEntryType type;
    Result rc = fsFsGetEntryType(m_fs, FixPath(path), &type);
    if (R_SUCCEEDED(rc)) {
        *out_entry_type = (type == FsDirEntryType_Dir) ? haze::FileAttrType_DIR : haze::FileAttrType_FILE;
    }
    return rc;
}

Result NxModManagerProxy::GetEntryAttributes(const char *path, haze::FileAttr *out) {
    // 获取文件类型
    FsDirEntryType type;
    Result rc = fsFsGetEntryType(m_fs, FixPath(path), &type);
    if (R_FAILED(rc)) {
        return rc;
    }

    if (type == FsDirEntryType_File) {
        out->type = haze::FileAttrType_FILE;

        // 获取时间戳（如果失败也不影响）
        FsTimeStampRaw timestamp{};
        if (R_SUCCEEDED(fsFsGetFileTimeStampRaw(m_fs, FixPath(path), &timestamp)) && timestamp.is_valid) {
            out->ctime = timestamp.created;
            out->mtime = timestamp.modified;
        }

        // 获取文件大小
        FsFile file;
        rc = fsFsOpenFile(m_fs, FixPath(path), FsOpenMode_Read, &file);
        if (R_SUCCEEDED(rc)) {
            s64 size;
            if (R_SUCCEEDED(fsFileGetSize(&file, &size))) {
                out->size = size;
            }
            fsFileClose(&file);
        }
    } else {
        out->type = haze::FileAttrType_DIR;
    }

    // 设置只读标志（如果需要）
    out->flag = 0;  // 默认可读写

    return 0;  // 成功
}

// 文件操作
Result NxModManagerProxy::CreateFile(const char* path, s64 size) {
    u32 flags = 0;
    const s64 _4_GB = 0x100000000;
    if (size >= _4_GB) {
        flags = FsCreateOption_BigFile;
    }

    // 参考libhaze示例：不在这里设置大小，避免长时间阻塞导致超时
    // SEE: https://github.com/ITotalJustice/libhaze/issues/1#issuecomment-3305067733
    return fsFsCreateFile(m_fs, FixPath(path), 0, flags);
}

Result NxModManagerProxy::DeleteFile(const char* path) {
    return fsFsDeleteFile(m_fs, FixPath(path));
}

Result NxModManagerProxy::RenameFile(const char *old_path, const char *new_path) {
    char fixed_old[FS_MAX_PATH], fixed_new[FS_MAX_PATH];
    return fsFsRenameFile(m_fs, FixPath(old_path, fixed_old), FixPath(new_path, fixed_new));
}

Result NxModManagerProxy::OpenFile(const char *path, haze::FileOpenMode mode, haze::File *out_file) {
    // 转换haze::FileOpenMode到FsOpenMode
    u32 fs_mode = FsOpenMode_Read;
    if (mode == haze::FileOpenMode_WRITE) {
        fs_mode = FsOpenMode_Write | FsOpenMode_Append;
    }

    auto file = new FsFile();
    const auto rc = fsFsOpenFile(m_fs, FixPath(path), fs_mode, file);
    if (R_FAILED(rc)) {
        delete file;
        return rc;
    }

    out_file->impl = file;
    return 0;  // 成功
}

Result NxModManagerProxy::GetFileSize(haze::File *file, s64 *out_size) {
    auto f = static_cast<FsFile*>(file->impl);
    return fsFileGetSize(f, out_size);
}

Result NxModManagerProxy::SetFileSize(haze::File *file, s64 size) {
    // 参考libhaze示例：设置为0如果Switch在分配大文件时冻结
    // 这通常发生在使用emuMMC + Windows时
    #if 0
    auto f = static_cast<FsFile*>(file->impl);
    return fsFileSetSize(f, size);
    #else
    return 0;  // 成功，但不实际设置大小
    #endif
}

Result NxModManagerProxy::ReadFile(haze::File *file, s64 off, void *buf, u64 read_size, u64 *out_bytes_read) {
    auto f = static_cast<FsFile*>(file->impl);
    return fsFileRead(f, off, buf, read_size, FsReadOption_None, out_bytes_read);
}

Result NxModManagerProxy::WriteFile(haze::File *file, s64 off, const void *buf, u64 write_size) {
    auto f = static_cast<FsFile*>(file->impl);
    
    return fsFileWrite(f, off, buf, write_size, FsWriteOption_None);
}

void NxModManagerProxy::CloseFile(haze::File *file) {
    auto f = static_cast<FsFile*>(file->impl);
    if (f) {
        fsFileClose(f);
        delete f;
        file->impl = nullptr;
    }
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

Result NxModManagerProxy::OpenDirectory(const char *path, haze::Dir *out_dir) {
    auto dir = new FsDir();
    const auto rc = fsFsOpenDirectory(m_fs, FixPath(path), FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles | FsDirOpenMode_NoFileSize, dir);
    if (R_FAILED(rc)) {
        delete dir;
        return rc;
    }

    out_dir->impl = dir;
    return 0;  // 成功
}

Result NxModManagerProxy::ReadDirectory(haze::Dir *d, s64 *out_total_entries, size_t max_entries, haze::DirEntry *buf) {
    auto dir = static_cast<FsDir*>(d->impl);

    std::vector<FsDirectoryEntry> entries(max_entries);
    Result rc = fsDirRead(dir, out_total_entries, entries.size(), entries.data());
    if (R_SUCCEEDED(rc)) {
        for (s64 i = 0; i < *out_total_entries; i++) {
            std::strcpy(buf[i].name, entries[i].name);
        }
    }

    return rc;
}

Result NxModManagerProxy::GetDirectoryEntryCount(haze::Dir *d, s64 *out_count) {
    auto dir = static_cast<FsDir*>(d->impl);
    return fsDirGetEntryCount(dir, out_count);
}

void NxModManagerProxy::CloseDirectory(haze::Dir *d) {
    auto dir = static_cast<FsDir*>(d->impl);
    if (dir) {
        fsDirClose(dir);
        delete dir;
        d->impl = nullptr;
    }
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
            // 读取开始，记录文件名并初始化速度计算
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
            break;
            
        case haze::CallbackType_WriteBegin:
            // 写入开始，记录文件名并初始化速度计算
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