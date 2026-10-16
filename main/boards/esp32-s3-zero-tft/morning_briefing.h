#pragma once
#include <sdkconfig.h>
#if CONFIG_BOARD_TYPE_ESP32_S3_ZERO_TFT
#include <cstdint>
#include <string>

// Device-local, single-task morning program. No HTTP, TTS, LVGL, or D1
// dependencies: Application forwards protocol events; home task only schedules
// an entry point. The class is built solely for Hi Lily N4R2.
class MorningBriefing {
public:
    enum class Advance { None, Next, Finished, ChannelLost };
    bool Active() const { return part_ != 0; }
    uint8_t Part() const { return part_; }
    bool TimedOut(int64_t now_us) const;
    bool StartedBySpeech(const std::string& text, int64_t now_us);
    bool ShouldStopForSpeech(const std::string& text) const;
    void BeginScheduled(const std::string& vietnam_day, int64_t now_us);
    void TtsStarted(int64_t now_us);
    void TtsStopped();
    Advance NextIfReady(bool playback_idle, bool channel_open,
                        int64_t now_us, std::string& next_text);
    void Cancel();
    static const char* InitialRequest();
private:
    void Start(int64_t now_us);
    uint8_t part_ = 0;
    bool tts_started_ = false;
    bool tts_stopped_ = false;
    bool awaiting_response_ = false;
    int64_t last_activity_us_ = 0;
    std::string scheduled_day_;
};
#endif
