#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Bounded HTTP producer -> MP3 decoder consumer.
// The ring belongs to one playback session; Stop() joins the producer before freeing PSRAM.
class HiLilyStreamSource {
public:
    HiLilyStreamSource() = default;
    ~HiLilyStreamSource();
    bool Start(std::string url, bool live);
    // >0 bytes, 0 normal EOF, -1 error/cancel. Timeout is bounded.
    // Returns -2 when interrupted by voice priority/pause, without consuming bytes.
    int Read(uint8_t* dst, size_t capacity, const std::function<bool()>& interrupt);
    void Stop();
    size_t Buffered() const;
    bool IsLive() const { return live_; }
private:
    static void Entry(void* arg);
    void Run();
    size_t Write(const uint8_t* src, size_t size);
    mutable std::mutex mutex_;
    uint8_t* ring_ = nullptr;
    const size_t capacity_ = 48 * 1024;
    size_t rd_ = 0, wr_ = 0, used_ = 0;
    std::string url_;
    bool live_ = false, ended_ = false, error_ = false;
    std::atomic<bool> stop_{false};
    // Only the producer signals finished, after its last access to this object.
    std::atomic<bool> finished_{true};
};
