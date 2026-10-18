#ifndef HILILY_MUSIC_PLAYER_H
#define HILILY_MUSIC_PLAYER_H
#include <cstdint>
#include <atomic>
#include <mutex>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Board-only direct Worker MP3 player. Network and I2S operations run off the
// MCP/server task. All control methods return immediately.
class HiLilyMusicPlayer {
public:
    static HiLilyMusicPlayer& GetInstance();
    void Configure(const char* worker_base);
    bool Play(std::string title, std::string artist = {});
    bool PlayRadio(std::string station);
    bool Pause();
    bool Resume();
    bool Stop();
    std::string StatusJson();
    bool IsBusy(); // Loading, playing or paused music blocks morning radio
    // TTS and spoken notifications take priority over streamed audio.
    void SetTtsActive(bool active);
    void SetNotificationActive(bool active);
    bool HasVoicePriority() const;
    // Optional shortcut for finalized Vietnamese STT commands.
    // This executes locally, without waiting for an LLM tool invocation.
    bool HandleRecognizedSpeech(const std::string& recognized_text);
private:
    HiLilyMusicPlayer() = default;
    static void WorkerEntry(void* arg);
    void Worker();
    bool StreamOne(uint32_t generation, const std::string& title, const std::string& artist);
    bool Cancelled(uint32_t generation);
    bool AwaitPlay(uint32_t generation);
    void UpdatePosition(uint32_t generation, uint32_t elapsed_ms);
    std::mutex mutex_;
    std::string worker_base_, requested_title_, requested_artist_;
    bool worker_running_ = false; // guarded by mutex_; set before task creation
    uint32_t generation_ = 0;
    bool pending_ = false, paused_ = false, active_ = false;
    std::atomic<bool> tts_active_{false};
    std::atomic<bool> notification_active_{false};
    uint32_t position_ms_ = 0;
    std::string playback_error_;
};
#endif
