/**
 * @file audio_manager.cpp
 * @brief 音效管理器类的实现
 */
#include "audio_manager.hpp"
#include <cstring>

// AudioManager::AudioManager() 
//     : m_initialized(false)
//     , m_keySoundId(PLSR_PLAYER_INVALID_SOUND)
//     , m_confirmSoundId(PLSR_PLAYER_INVALID_SOUND)
//     , m_cancelSoundId(PLSR_PLAYER_INVALID_SOUND)
//     , m_scrollSoundId(PLSR_PLAYER_INVALID_SOUND)
//     , m_limitSoundId(PLSR_PLAYER_INVALID_SOUND)
//     , m_startupSoundId(PLSR_PLAYER_INVALID_SOUND) {
//     // 初始化BFSAR结构
//     memset(&m_bfsar, 0, sizeof(m_bfsar));
// }

AudioManager::AudioManager() 
    : m_initialized(false)
    , m_async_init_started(false)
    , m_stop_token(m_stop_source.get_token())
    , m_keySoundId(PLSR_PLAYER_INVALID_SOUND)
    , m_confirmSoundId(PLSR_PLAYER_INVALID_SOUND)
    , m_cancelSoundId(PLSR_PLAYER_INVALID_SOUND)
    , m_limitSoundId(PLSR_PLAYER_INVALID_SOUND) {
    // 初始化BFSAR结构
    memset(&m_bfsar, 0, sizeof(m_bfsar));
}

AudioManager::~AudioManager() {
    // 请求停止异步初始化（如果正在进行）
    if (m_async_init_started) {
        RequestStop();
    }
    
    // 如果异步初始化正在进行，等待其完成
    if (m_async_init_started && m_async_init_future.valid()) {
        m_async_init_future.wait();
    }
    
    Cleanup();
}

bool AudioManager::Initialize() {
    if (m_initialized) {
        return true;
    }
    
    // 初始化pulsar播放器
    PLSR_RC rc = plsrPlayerInit();
    if (rc != PLSR_RC_OK) {
        return false;
    }
    
    // 挂载qlaunch的ROMFS存储（包含系统音效）
    Result result = romfsMountDataStorageFromProgram(0x0100000000001000, "qlaunch");
    if (R_FAILED(result)) {
        plsrPlayerExit();
        return false;
    }
    
    // 打开qlaunch音效档案
    rc = plsrBFSAROpen("qlaunch:/sound/qlaunch.bfsar", &m_bfsar);
    if (rc != PLSR_RC_OK) {
        romfsUnmount("qlaunch");
        plsrPlayerExit();
        return false;
    }
    
    // 加载系统音效
    if (!LoadSystemSounds()) {
        plsrBFSARClose(&m_bfsar);
        romfsUnmount("qlaunch");
        plsrPlayerExit();
        return false;
    }
    
    m_initialized = true;
    return true;
}

void AudioManager::InitializeAsync() {
    if (m_async_init_started || m_initialized) {
        return;
    }
    
    m_async_init_started = true;
    
    // 启动异步初始化任务
    m_async_init_future = util::async([this](std::stop_token stop_token) {
        // 检查是否需要停止
        if (stop_token.stop_requested()) {
            return;
        }
        
        // 初始化pulsar播放器
        PLSR_RC rc = plsrPlayerInit();
        if (rc != PLSR_RC_OK) {
            return;
        }
        
        if (stop_token.stop_requested()) {
            romfsUnmount("qlaunch");
            plsrPlayerExit();
            return;
        }
        
        // 挂载qlaunch的ROMFS存储（包含系统音效）
        Result result = romfsMountDataStorageFromProgram(0x0100000000001000, "qlaunch");
        if (R_FAILED(result)) {
            plsrPlayerExit();
            return;
        }
        
        if (stop_token.stop_requested()) {
            plsrPlayerExit();
            return;
        }
        
        // 打开qlaunch音效档案
        rc = plsrBFSAROpen("qlaunch:/sound/qlaunch.bfsar", &m_bfsar);
        if (rc != PLSR_RC_OK) {
            plsrPlayerExit();
            return;
        }
        
        if (stop_token.stop_requested()) {
            plsrBFSARClose(&m_bfsar);
            romfsUnmount("qlaunch");
            plsrPlayerExit();
            return;
        }
        
        // 加载系统音效
        if (!LoadSystemSounds()) {
            plsrBFSARClose(&m_bfsar);
            romfsUnmount("qlaunch");
            plsrPlayerExit();
            return;
        }
        
        // 标记初始化完成
        m_initialized = true;
    }, m_stop_token);
}

bool AudioManager::IsAsyncInitComplete() const {
    if (!m_async_init_started) {
        return false;
    }
    
    // 检查异步任务是否完成
    if (m_async_init_future.valid()) {
        auto status = m_async_init_future.wait_for(std::chrono::seconds(0));
        return status == std::future_status::ready && m_initialized;
    }
    
    return m_initialized;
}

void AudioManager::RequestStop() {
    // 请求停止异步初始化
    m_stop_source.request_stop();
}

void AudioManager::Cleanup() {
    // 防止重复清理
    if (!m_initialized) {
        return;
    }
    
    // 首先标记为未初始化，防止其他线程访问
    m_initialized = false;
    
    // 释放音效资源
    if (m_keySoundId != PLSR_PLAYER_INVALID_SOUND) {
        plsrPlayerFree(m_keySoundId);
        m_keySoundId = PLSR_PLAYER_INVALID_SOUND;
    }
    
    if (m_confirmSoundId != PLSR_PLAYER_INVALID_SOUND) {
        plsrPlayerFree(m_confirmSoundId);
        m_confirmSoundId = PLSR_PLAYER_INVALID_SOUND;
    }
    
    if (m_cancelSoundId != PLSR_PLAYER_INVALID_SOUND) {
        plsrPlayerFree(m_cancelSoundId);
        m_cancelSoundId = PLSR_PLAYER_INVALID_SOUND;
    }
    
    // if (m_scrollSoundId != PLSR_PLAYER_INVALID_SOUND) {
    //     plsrPlayerFree(m_scrollSoundId);
    //     m_scrollSoundId = PLSR_PLAYER_INVALID_SOUND;
    // }
    
    if (m_limitSoundId != PLSR_PLAYER_INVALID_SOUND) {
        plsrPlayerFree(m_limitSoundId);
        m_limitSoundId = PLSR_PLAYER_INVALID_SOUND;
    }
    
    // if (m_startupSoundId != PLSR_PLAYER_INVALID_SOUND) {
    //     plsrPlayerFree(m_startupSoundId);
    //     m_startupSoundId = PLSR_PLAYER_INVALID_SOUND;
    // }
    
    // 关闭音效档案
    plsrBFSARClose(&m_bfsar);

    // 卸载ROMFS
    romfsUnmount("qlaunch");

    // 清理播放器
    plsrPlayerExit();
}

bool AudioManager::LoadSystemSounds() {
    // 直接加载到原有的音效变量中
    
    // 加载按键音效(Load key sound effect) - 焦点音效(Focus sound)
    plsrPlayerLoadSoundByName(&m_bfsar, "SeGameIconFocus", &m_keySoundId);
    
    // 加载确认音效(Load confirm sound effect) - 安装音效(Install sound)
    plsrPlayerLoadSoundByName(&m_bfsar, "SeGameIconAdd", &m_confirmSoundId);
    
    // 加载取消音效(Load cancel sound effect) - 错误音效(Error sound)
    plsrPlayerLoadSoundByName(&m_bfsar, "SeInsertError", &m_cancelSoundId);
    
    // 加载其他音效到新的变量中(Load other sound effects to new variables)
    // plsrPlayerLoadSoundByName(&m_bfsar, "SeGameIconScroll", &m_scrollSoundId);  // 滚动音效(Scroll sound)
    plsrPlayerLoadSoundByName(&m_bfsar, "SeGameIconLimit", &m_limitSoundId);    // 限制音效(Limit sound)
    // plsrPlayerLoadSoundByName(&m_bfsar, "StartupMenu_Game", &m_startupSoundId); // 启动音效(Startup sound)
    
    // 检查是否至少有一个音效加载成功
    return (m_keySoundId != PLSR_PLAYER_INVALID_SOUND || 
            m_confirmSoundId != PLSR_PLAYER_INVALID_SOUND ||
            m_cancelSoundId != PLSR_PLAYER_INVALID_SOUND ||
            m_limitSoundId != PLSR_PLAYER_INVALID_SOUND);
}

void AudioManager::PlayKeySound(float volume) {
    if (!m_initialized || m_keySoundId == PLSR_PLAYER_INVALID_SOUND) {
        return;
    }
    
    // 防抖检查：避免按键音效过于频繁播放 (Debounce check: avoid key sound playing too frequently)
    auto now = std::chrono::steady_clock::now();
    if (now - m_lastKeySoundTime < AUDIO_DEBOUNCE_MS) {
        return; // 距离上次播放时间太短，跳过本次播放 (Too soon since last play, skip this play)
    }
    
    // 设置音量
    plsrPlayerSetVolume(m_keySoundId, volume);
    
    // 播放音效
    plsrPlayerPlay(m_keySoundId);
    
    // 更新最后播放时间 (Update last play time)
    m_lastKeySoundTime = now;
}

void AudioManager::PlayConfirmSound(float volume) {
    if (!m_initialized || m_confirmSoundId == PLSR_PLAYER_INVALID_SOUND) {
        return;
    }
    
    // 确认音效使用较短的防抖时间，保持响应性 (Confirm sound uses shorter debounce time for responsiveness)
    auto now = std::chrono::steady_clock::now();
    constexpr auto CONFIRM_DEBOUNCE_MS = std::chrono::milliseconds(80);
    if (now - m_lastConfirmSoundTime < CONFIRM_DEBOUNCE_MS) {
        return; // 距离上次播放时间太短，跳过本次播放 (Too soon since last play, skip this play)
    }
    
    // 设置音量
    plsrPlayerSetVolume(m_confirmSoundId, volume);
    
    // 播放音效
    plsrPlayerPlay(m_confirmSoundId);
    
    // 更新最后播放时间 (Update last play time)
    m_lastConfirmSoundTime = now;
}

void AudioManager::PlayCancelSound(float volume) {
    if (!m_initialized || m_cancelSoundId == PLSR_PLAYER_INVALID_SOUND) {
        return;
    }
    
    // 取消音效使用较短的防抖时间，保持响应性 (Cancel sound uses shorter debounce time for responsiveness)
    auto now = std::chrono::steady_clock::now();
    constexpr auto CANCEL_DEBOUNCE_MS = std::chrono::milliseconds(80);
    if (now - m_lastCancelSoundTime < CANCEL_DEBOUNCE_MS) {
        return; // 距离上次播放时间太短，跳过本次播放 (Too soon since last play, skip this play)
    }
    
    // 设置音量并播放音效
    plsrPlayerSetVolume(m_cancelSoundId, volume);
    plsrPlayerPlay(m_cancelSoundId);
    
    // 更新最后播放时间 (Update last play time)
    m_lastCancelSoundTime = now;
}

// void AudioManager::PlayScrollSound(float volume) {
//     if (!m_initialized || m_scrollSoundId == PLSR_PLAYER_INVALID_SOUND) {
//         return;
//     }
    
//     // 设置音量并播放滚动音效
//     plsrPlayerSetVolume(m_scrollSoundId, volume);
//     plsrPlayerPlay(m_scrollSoundId);
// }

void AudioManager::PlayLimitSound(float volume) {
    if (!m_initialized || m_limitSoundId == PLSR_PLAYER_INVALID_SOUND) {
        return;
    }
    
    // 限制音效使用标准防抖时间 (Limit sound uses standard debounce time)
    auto now = std::chrono::steady_clock::now();
    constexpr auto CANCEL_DEBOUNCE_MS = std::chrono::milliseconds(200);
    if (now - m_lastLimitSoundTime < CANCEL_DEBOUNCE_MS) {
        return; // 距离上次播放时间太短，跳过本次播放 (Too soon since last play, skip this play)
    }
    
    // 设置音量并播放限制/边界音效
    plsrPlayerSetVolume(m_limitSoundId, volume);
    plsrPlayerPlay(m_limitSoundId);
    
    // 更新最后播放时间 (Update last play time)
    m_lastLimitSoundTime = now;
}

// void AudioManager::PlayStartupSound(float volume) {
//     if (!m_initialized || m_startupSoundId == PLSR_PLAYER_INVALID_SOUND) {
//         return;
//     }
    
//     // 设置音量并播放启动音效
//     plsrPlayerSetVolume(m_startupSoundId, volume);
//     plsrPlayerPlay(m_startupSoundId);
// }