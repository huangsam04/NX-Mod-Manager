#pragma once

#include <switch.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace tj {

// 音效类型枚举 (Audio effect type enumeration)
enum class AudioType {
    BUTTON_CLICK,    // 按钮点击音效 (Button click sound effect)
    BUTTON_HOVER,    // 按钮悬停音效 (Button hover sound effect)
    CONFIRM,         // 确认音效 (Confirmation sound effect)
    CANCEL,          // 取消音效 (Cancel sound effect)
    ERROR,           // 错误音效 (Error sound effect)
    SUCCESS,         // 成功音效 (Success sound effect)
    NOTIFICATION,    // 通知音效 (Notification sound effect)
};

// 音频数据结构体 (Audio data structure)
struct AudioData {
    std::unique_ptr<u8[]> data;  // 音频数据指针 (Audio data pointer)
    size_t size;                 // 数据大小 (Data size)
    u32 sample_rate;             // 采样率 (Sample rate)
    u16 channels;                // 声道数 (Number of channels)
    u16 bits_per_sample;         // 每样本位数 (Bits per sample)
    
    AudioData() = default;
    AudioData(const AudioData&) = delete;
    AudioData& operator=(const AudioData&) = delete;
    
    // 移动构造函数 (Move constructor)
    AudioData(AudioData&& other) noexcept
        : data(std::move(other.data))
        , size(other.size)
        , sample_rate(other.sample_rate)
        , channels(other.channels)
        , bits_per_sample(other.bits_per_sample) {
        other.size = 0;
        other.sample_rate = 0;
        other.channels = 0;
        other.bits_per_sample = 0;
    }
    
    // 移动赋值操作符 (Move assignment operator)
    AudioData& operator=(AudioData&& other) noexcept {
        if (this != &other) {
            data = std::move(other.data);
            size = other.size;
            sample_rate = other.sample_rate;
            channels = other.channels;
            bits_per_sample = other.bits_per_sample;
            
            other.size = 0;
            other.sample_rate = 0;
            other.channels = 0;
            other.bits_per_sample = 0;
        }
        return *this;
    }
};

// 音效管理器类 (Audio manager class)
class AudioManager {
public:
    AudioManager();
    ~AudioManager();
    
    // 禁止拷贝构造和赋值 (Disable copy construction and assignment)
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    
    // 初始化音频系统 (Initialize audio system)
    bool Initialize();
    
    // 清理音频系统 (Cleanup audio system)
    void Cleanup();
    
    // 播放指定类型的音效 (Play specified type of sound effect)
    void PlaySound(AudioType type);
    
    // 播放自定义音效文件 (Play custom sound effect file)
    bool PlaySoundFile(const std::string& file_path);
    
    // 设置主音量 (Set master volume)
    void SetMasterVolume(float volume); // 0.0f - 1.0f
    
    // 获取主音量 (Get master volume)
    float GetMasterVolume() const;
    
    // 设置音效开关 (Set sound effect on/off)
    void SetSoundEnabled(bool enabled);
    
    // 获取音效开关状态 (Get sound effect on/off status)
    bool IsSoundEnabled() const;
    
    // 预加载音效资源 (Preload sound effect resources)
    bool PreloadSounds();
    
    // 检查音频系统是否已初始化 (Check if audio system is initialized)
    bool IsInitialized() const;
    
private:
    bool initialized_{false};           // 是否已初始化 (Whether initialized)
    bool sound_enabled_{true};          // 音效是否启用 (Whether sound effects are enabled)
    float master_volume_{0.8f};         // 主音量 (Master volume)
    
    // 音效数据缓存 (Sound effect data cache)
    std::unordered_map<AudioType, AudioData> audio_cache_;
    mutable std::mutex audio_mutex_;    // 音频操作互斥锁 (Audio operation mutex)
    
    // 内部方法 (Internal methods)
    bool LoadAudioFile(const std::string& file_path, AudioData& audio_data);
    bool LoadDefaultSounds(); // 加载默认音效 (Load default sound effects)
    void PlayAudioData(const AudioData& audio_data);
    
    // 音频类型到文件路径的映射 (Mapping from audio type to file path)
    std::string GetAudioFilePath(AudioType type) const;
};

} // namespace tj