#include "hilily_stream_source.h"
#include "board.h"
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>

namespace {
constexpr char TAG[] = "HiLilySource";
constexpr unsigned kMaxReconnect = 3;
}

HiLilyStreamSource::~HiLilyStreamSource() { Stop(); }

bool HiLilyStreamSource::Start(std::string url, bool live) {
    if (!finished_.load() || ring_ || url.rfind("https://", 0) != 0 || url.size() > 900) return false;
    ring_ = static_cast<uint8_t*>(heap_caps_malloc(capacity_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!ring_) return false;
    url_ = std::move(url);
    live_ = live;
    stop_.store(false);
    finished_.store(false);
    TaskHandle_t handle = nullptr;
    BaseType_t result = xTaskCreate(Entry, "hilily_http", 6144, this, 3, &handle);
    if (result != pdPASS) {
        finished_.store(true);
        heap_caps_free(ring_);
        ring_ = nullptr;
        return false;
    }
    return true;
}

void HiLilyStreamSource::Stop() {
    stop_.store(true);
    // The producer owns HTTP and the ring; wait for its last object access.
    // No task handle is shared: avoids a create/exit handle-publishing race.
    while (!finished_.load()) vTaskDelay(pdMS_TO_TICKS(20));
    if (ring_) {
        heap_caps_free(ring_);
        ring_ = nullptr;
    }
}

size_t HiLilyStreamSource::Buffered() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return used_;
}

size_t HiLilyStreamSource::Write(const uint8_t* src, size_t n) {
    std::lock_guard<std::mutex> lock(mutex_);
    const size_t count = std::min(n, capacity_ - used_);
    const size_t a = std::min(count, capacity_ - wr_);
    std::memcpy(ring_ + wr_, src, a);
    std::memcpy(ring_, src + a, count - a);
    wr_ = (wr_ + count) % capacity_;
    used_ += count;
    return count;
}

int HiLilyStreamSource::Read(uint8_t* dst, size_t size,
                              const std::function<bool()>& interrupt) {
    const TickType_t start = xTaskGetTickCount();
    while (!stop_.load()) {
        if (interrupt && interrupt()) return -2;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (used_) {
                const size_t count = std::min(size, used_);
                const size_t a = std::min(count, capacity_ - rd_);
                std::memcpy(dst, ring_ + rd_, a);
                std::memcpy(dst + a, ring_, count - a);
                rd_ = (rd_ + count) % capacity_;
                used_ -= count;
                return static_cast<int>(count);
            }
            if (ended_) return 0;
            if (error_) return -1;
        }
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(20000)) return -1;
        vTaskDelay(pdMS_TO_TICKS(15));
    }
    return -1;
}

void HiLilyStreamSource::Entry(void* arg) {
    auto* source = static_cast<HiLilyStreamSource*>(arg);
    source->Run();
    source->finished_.store(true);
    // No further access to source after finished=true.
    vTaskDelete(nullptr);
}

void HiLilyStreamSource::Run() {
    uint64_t received = 0;
    unsigned retries = 0;
    bool completed = false;
    std::array<uint8_t, 2048> block{};
    while (!stop_.load()) {
        auto http = Board::GetInstance().GetNetwork()->CreateHttp(0);
        if (!http) break;
        http->SetTimeout(5000);
        http->SetHeader("Accept", "audio/mpeg, application/octet-stream");
        http->SetHeader("Accept-Encoding", "identity");
        http->SetHeader("Icy-MetaData", "0");
        // Byte-accurate MP3 continuation: only accept 206 when requesting Range.
        const bool ranged = !live_ && received != 0;
        if (ranged) http->SetHeader("Range", "bytes=" + std::to_string(received) + "-");
        const auto opened = http->Open("GET", url_);
        auto status = opened ? http->GetStatusCode() : decltype(http->GetStatusCode()){};
        const bool valid = opened && status && *status == (ranged ? 206 : 200);
        if (!valid) {
            ESP_LOGW(TAG, "stream connect failed (status=%d, range=%d)", status ? *status : -1, ranged);
            http->Close();
        } else {
            bool io_error = false;
            while (!stop_.load()) {
                if (Buffered() >= capacity_ - block.size()) {
                    vTaskDelay(pdMS_TO_TICKS(15));
                    continue;
                }
                auto count = http->Read(reinterpret_cast<char*>(block.data()), block.size());
                if (!count) { io_error = true; break; }
                if (*count == 0) {
                    if (!live_) completed = true;
                    else io_error = true;
                    break;
                }
                const size_t n = static_cast<size_t>(*count);
                size_t done = 0;
                while (done < n && !stop_.load()) {
                    size_t written = Write(block.data() + done, n - done);
                    done += written;
                    if (done < n) vTaskDelay(pdMS_TO_TICKS(10));
                }
                received += done;
                retries = 0;
            }
            http->Close();
            if (completed || stop_.load()) break;
            if (!io_error) break;
        }
        if (++retries > kMaxReconnect) break;
        ESP_LOGW(TAG, "reconnecting stream (%u/%u)", retries, kMaxReconnect);
        vTaskDelay(pdMS_TO_TICKS(400 * retries));
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ended_ = completed;
        error_ = !completed && !stop_.load();
    }
}
