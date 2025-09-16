// 定义NXLINK_LOG宏为0，用于控制是否启用网络日志功能
#define NXLINK_LOG 0

// 包含Switch系统的主要头文件，提供系统API
#include <switch.h>
// 包含C标准库，提供内存分配等功能
#include <cstdlib>
// 包含字符串处理函数
#include <cstring>
// 包含标准输入输出函数
#include <cstdio>
// 如果启用了NXLINK_LOG，则包含UNIX标准定义
#if NXLINK_LOG
#include <unistd.h>
#endif
// 包含haze库的头文件，提供MTP服务器功能
#include "haze.h"

// 匿名命名空间，用于定义内部使用的变量和函数
namespace {

// 全局互斥锁，用于保护回调数据的线程安全访问
Mutex g_mutex;
// 全局向量，存储来自haze库的回调数据
std::vector<haze::CallbackData> g_callback_data;

// FsNative结构体，继承自haze::FileSystemProxyImpl，实现文件系统代理
struct FsNative : haze::FileSystemProxyImpl {
    // 默认构造函数
    FsNative() = default;
    // 带参数的构造函数，接收文件系统指针和所有权标志
    FsNative(FsFileSystem* fs, bool own) {
        // 复制文件系统结构体
        m_fs = *fs;
        // 设置是否拥有文件系统的所有权
        m_own = own;
    }

    // 析构函数，负责清理文件系统资源
    ~FsNative() {
        // 提交文件系统的所有待处理操作
        fsFsCommit(&m_fs);
        // 如果拥有文件系统所有权，则关闭文件系统
        if (m_own) {
            fsFsClose(&m_fs);
        }
    }

    // 修正路径函数，将MTP路径转换为文件系统路径
    auto FixPath(const char* path, char* out = nullptr) const -> const char* {
        // 静态缓冲区，用于存储修正后的路径
        static char buf[FS_MAX_PATH];
        // 获取文件系统名称的长度
        const auto len = std::strlen(GetName());

        // 如果没有提供输出缓冲区，使用默认缓冲区
        if (!out) {
            out = buf;
        }

        // 如果路径包含文件系统名称前缀，则移除该前缀
        if (len && !strncasecmp(path + 1, GetName(), len)) {
            std::snprintf(out, sizeof(buf), "/%s", path + 1 + len);
        } else {
            // 否则直接复制原路径
            std::strcpy(out, path);
        }

        // 返回修正后的路径
        return out;
    }

    // 获取指定路径的总空间大小
    Result GetTotalSpace(const char *path, s64 *out) override {
        return fsFsGetTotalSpace(&m_fs, FixPath(path), out);
    }
    // 获取指定路径的可用空间大小
    Result GetFreeSpace(const char *path, s64 *out) override {
        return fsFsGetFreeSpace(&m_fs, FixPath(path), out);
    }
    // 获取指定路径的条目类型（文件或目录）
    Result GetEntryType(const char *path, FsDirEntryType *out_entry_type) override {
        return fsFsGetEntryType(&m_fs, FixPath(path), out_entry_type);
    }
    // 创建指定路径和大小的文件
    Result CreateFile(const char* path, s64 size, u32 option) override {
        return fsFsCreateFile(&m_fs, FixPath(path), size, option);
    }
    // 删除指定路径的文件
    Result DeleteFile(const char* path) override {
        return fsFsDeleteFile(&m_fs, FixPath(path));
    }
    // 重命名文件，从旧路径移动到新路径
    Result RenameFile(const char *old_path, const char *new_path) override {
        // 临时缓冲区用于存储修正后的旧路径
        char temp[FS_MAX_PATH];
        return fsFsRenameFile(&m_fs, FixPath(old_path, temp), FixPath(new_path));
    }
    // 以指定模式打开文件
    Result OpenFile(const char *path, u32 mode, FsFile *out_file) override {
        return fsFsOpenFile(&m_fs, FixPath(path), mode, out_file);
    }
    // 获取文件的大小
    Result GetFileSize(FsFile *file, s64 *out_size) override {
        return fsFileGetSize(file, out_size);
    }
    // 设置文件的大小
    Result SetFileSize(FsFile *file, s64 size) override {
        return fsFileSetSize(file, size);
    }
    // 从文件的指定偏移位置读取数据到缓冲区
    Result ReadFile(FsFile *file, s64 off, void *buf, u64 read_size, u32 option, u64 *out_bytes_read) override {
        return fsFileRead(file, off, buf, read_size, option, out_bytes_read);
    }
    // 将缓冲区数据写入文件的指定偏移位置
    Result WriteFile(FsFile *file, s64 off, const void *buf, u64 write_size, u32 option) override {
        return fsFileWrite(file, off, buf, write_size, option);
    }
    // 关闭文件句柄
    void CloseFile(FsFile *file) override {
        fsFileClose(file);
    }

    // 创建目录
    Result CreateDirectory(const char* path) override {
        return fsFsCreateDirectory(&m_fs, FixPath(path));
    }
    // 递归删除目录及其所有内容
    Result DeleteDirectoryRecursively(const char* path) override {
        return fsFsDeleteDirectoryRecursively(&m_fs, FixPath(path));
    }
    // 重命名目录，从旧路径移动到新路径
    Result RenameDirectory(const char *old_path, const char *new_path) override {
        // 临时缓冲区用于存储修正后的旧路径
        char temp[FS_MAX_PATH];
        return fsFsRenameDirectory(&m_fs, FixPath(old_path, temp), FixPath(new_path));
    }
    // 以指定模式打开目录
    Result OpenDirectory(const char *path, u32 mode, FsDir *out_dir) override {
        return fsFsOpenDirectory(&m_fs, FixPath(path), mode, out_dir);
    }
    // 读取目录中的条目到缓冲区
    Result ReadDirectory(FsDir *d, s64 *out_total_entries, size_t max_entries, FsDirectoryEntry *buf) override {
        return fsDirRead(d, out_total_entries, max_entries, buf);
    }
    // 获取目录中条目的总数
    Result GetDirectoryEntryCount(FsDir *d, s64 *out_count) override {
        return fsDirGetEntryCount(d, out_count);
    }
    // 关闭目录句柄
    void CloseDirectory(FsDir *d) override {
        fsDirClose(d);
    }

    // 文件系统结构体，存储底层文件系统信息
    FsFileSystem m_fs{};
    // 标志位，表示是否拥有文件系统的所有权
    bool m_own{true};
};

// FsSdmc结构体，继承自FsNative，用于访问SD卡文件系统
struct FsSdmc final : FsNative {
    // 构造函数，初始化SD卡文件系统，不拥有所有权
    FsSdmc() : FsNative(fsdevGetDeviceFileSystem("sdmc"), false) {
    }

    // 返回文件系统名称（空字符串表示根目录）
    const char* GetName() const {
        return "";
    }
    // 返回文件系统的显示名称
    const char* GetDisplayName() const {
        return "micro SD Card";
    }
};

// FsAlbum结构体，继承自FsNative，用于访问相册文件系统
struct FsAlbum final : FsNative {
    // 构造函数，打开指定ID的图像目录文件系统
    FsAlbum(FsImageDirectoryId id) {
        fsOpenImageDirectoryFileSystem(&m_fs, id);
    }

    // 返回文件系统名称（相册路径前缀）
    const char* GetName() const {
        return "album:/";
    }
    // 返回文件系统的显示名称
    const char* GetDisplayName() const {
        return "Album";
    }
};

// 回调处理函数，接收来自haze库的事件通知
void callbackHandler(const haze::CallbackData* data) {
    // 加锁保护共享数据
    mutexLock(&g_mutex);
        // 将回调数据添加到全局向量中
        g_callback_data.emplace_back(*data);
    // 解锁
    mutexUnlock(&g_mutex);
}

// 处理事件函数，显示MTP操作的日志信息
void processEvents() {
    // 创建本地数据向量用于存储回调数据
    std::vector<haze::CallbackData> data;

    // 加锁并交换数据，避免长时间持有锁
    mutexLock(&g_mutex);
        // 将全局回调数据交换到本地向量中
        std::swap(data, g_callback_data);
    // 解锁
    mutexUnlock(&g_mutex);

    // 遍历并记录所有事件
    for (const auto& e : data) {
        // 根据事件类型输出相应的日志信息
        switch (e.type) {
            // MTP会话开始
            case haze::CallbackType_OpenSession: std::printf("Opening Session\n"); break;
            // MTP会话结束
            case haze::CallbackType_CloseSession: std::printf("Closing Session\n"); break;

            // 创建文件操作
            case haze::CallbackType_CreateFile: std::printf("Creating File: %s\n", e.file.filename); break;
            // 删除文件操作
            case haze::CallbackType_DeleteFile: std::printf("Deleting File: %s\n", e.file.filename); break;

            // 重命名文件操作
            case haze::CallbackType_RenameFile: std::printf("Rename File: %s -> %s\n", e.rename.filename, e.rename.newname); break;
            // 重命名文件夹操作
            case haze::CallbackType_RenameFolder: std::printf("Rename Folder: %s -> %s\n", e.rename.filename, e.rename.newname); break;

            // 创建文件夹操作
            case haze::CallbackType_CreateFolder: std::printf("Creating Folder: %s\n", e.file.filename); break;
            // 删除文件夹操作
            case haze::CallbackType_DeleteFolder: std::printf("Deleting Folder: %s\n", e.file.filename); break;

            // 文件读取开始
            case haze::CallbackType_ReadBegin: std::printf("Reading File Begin: %s \r", e.file.filename); break;
            // 文件读取进度更新
            case haze::CallbackType_ReadProgress: std::printf("Reading File: offset: %lld size: %lld\r", e.progress.offset, e.progress.size); break;
            // 文件读取完成
            case haze::CallbackType_ReadEnd: std::printf("Reading File Finished: %s\n", e.file.filename); break;

            // 文件写入开始
            case haze::CallbackType_WriteBegin: std::printf("Writing File Begin: %s \r", e.file.filename); break;
            // 文件写入进度更新
            case haze::CallbackType_WriteProgress: std::printf("Writing File: offset: %lld size: %lld\r", e.progress.offset, e.progress.size); break;
            // 文件写入完成
            case haze::CallbackType_WriteEnd: std::printf("Writing File Finished: %s\n", e.file.filename); break;
        }
    }

    // 更新控制台显示
    consoleUpdate(NULL);
}

} // namespace

// 主函数，程序入口点
int main(int argc, char** argv) {
    // 如果启用了NXLINK_LOG，初始化网络套接字用于远程调试
    #if NXLINK_LOG
    socketInitializeDefault();
    int fd = nxlinkStdio();
    #endif

    // 创建文件系统条目列表
    haze::FsEntries fs_entries;
    // 添加SD卡文件系统
    fs_entries.emplace_back(std::make_shared<FsSdmc>());
    // 添加相册文件系统（SD卡上的相册）
    fs_entries.emplace_back(std::make_shared<FsAlbum>(FsImageDirectoryId_Sd));

    // 初始化互斥锁
    mutexInit(&g_mutex);
    // 初始化haze库，设置回调函数、厂商ID(0x2C)、产品ID(2)和文件系统列表
    haze::Initialize(callbackHandler, 0x2C, 2, fs_entries);
    // 初始化控制台用于屏幕显示
    consoleInit(NULL);

    // 初始化控制器输入
    PadState pad;
    // 配置输入，支持1个玩家，使用标准手柄样式
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    // 初始化默认手柄
    padInitializeDefault(&pad);

    // 显示程序信息和操作提示
    std::printf("libhaze example!\n\nPress (+) to exit\n");
    // 更新控制台显示
    consoleUpdate(NULL);

    // 主循环，直到按下+按钮退出
    while (appletMainLoop()) {
        // 更新手柄状态
        padUpdate(&pad);

        // 获取当前帧按下的按钮
        const u64 kDown = padGetButtonsDown(&pad);
        // 如果按下了+按钮，退出循环返回到hbmenu
        if (kDown & HidNpadButton_Plus)
            break;

        // 处理MTP事件并显示日志
        processEvents();
        // 休眠1/60秒，控制帧率为60FPS
        svcSleepThread(1e+9/60);
    }

    // 如果启用了NXLINK_LOG，关闭网络连接并清理套接字
    #if NXLINK_LOG
    close(fd);
    socketExit();
    #endif
    // 退出控制台显示
    consoleExit(NULL);
    // 通知haze库退出，关闭MTP服务线程
    haze::Exit();
}

// C语言接口块，提供应用程序生命周期回调函数
extern "C" {

// 在main函数调用前执行的初始化函数
void userAppInit(void) {
    // 锁定退出，阻止应用程序退出直到所有资源清理完成
    appletLockExit();
}

// 在main函数退出后执行的清理函数
void userAppExit(void) {
    // 解锁退出，允许应用程序干净地退出
    appletUnlockExit();
}

} // extern "C"
