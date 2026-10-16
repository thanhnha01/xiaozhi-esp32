#include "morning_briefing.h"
#if CONFIG_BOARD_TYPE_ESP32_S3_ZERO_TFT
#include <utility>

namespace {
constexpr uint8_t kPartCount = 12;
constexpr int64_t kResponseTimeoutUs = 120LL * 1000000;
constexpr int64_t kPlaybackTimeoutUs = 240LL * 1000000;
bool IsRequest(const std::string& text) {
    const char* keys[] = {"bản tin sáng", "Bản tin sáng", "bảng tin sáng",
        "Bảng tin sáng", "bản tin buổi sáng", "Bản tin buổi sáng",
        "điểm tin hôm nay", "Điểm tin hôm nay"};
    for (const char* key : keys)
        if (text.find(key) != std::string::npos) return true;
    return false;
}
bool IsAutomaticFollowup(const std::string& text) {
    return text.find("TT_BRIEF_AUTO") != std::string::npos;
}
std::string Followup(uint8_t part) {
    static const char* topics[kPartCount-1] = {
        "nhịp một, dự báo mưa theo giờ, AQI và lời dặn sinh hoạt",
        "nhịp hai, Can Chi, Trực ngày, giờ tốt, hướng xuất hành",
        "nhịp hai, diễn giải ý nghĩa ngày và việc nên hoặc không nên",
        "nhịp hai, giỗ chạp, ngày lễ, Rằm, Mùng Một và chuẩn bị sự kiện",
        "nhịp ba, lịch gia đình và việc chung",
        "nhịp ba, lời nhắn thành viên có sự kiện quan trọng",
        "nhịp ba, lời dặn thành viên còn lại nếu có dữ liệu",
        "nhịp bốn, tin số một và số hai với nội dung và nguồn",
        "nhịp bốn, tin số ba và số bốn với nội dung và nguồn",
        "nhịp bốn, tin số năm nếu có đủ nguồn xác thực",
        "nhịp năm, lời chúc ngắn gọn và kết thúc"
    };
    if (part < 2 || part > kPartCount) return {};
    return "TT_BRIEF_AUTO tiếp tục bản tin sáng đoạn " + std::to_string(part) +
        " trên 12. Nội dung là " + topics[part - 2] +
        ". Chỉ đọc đoạn này khoảng 100 đến 150 từ. Không chào lại,"
        " không lặp ý, không hỏi có muốn nghe tiếp, không đọc mã điều khiển."
        " Dùng dữ liệu MCP thực tế, nếu thiếu thì nói ngắn gọn, không bịa.";
}
}  // namespace

void MorningBriefing::Start(int64_t now_us) {
    part_ = 1;
    tts_started_ = false;
    tts_stopped_ = false;
    awaiting_response_ = true;
    last_activity_us_ = now_us;
}

bool MorningBriefing::StartedBySpeech(const std::string& text, int64_t now_us) {
    if (Active() || !IsRequest(text) || IsAutomaticFollowup(text)) return false;
    scheduled_day_.clear();
    Start(now_us);
    return true;
}

bool MorningBriefing::ShouldStopForSpeech(const std::string& text) const {
    return Active() && !IsAutomaticFollowup(text) &&
        (text == "dừng bản tin" || text == "Dừng bản tin");
}

void MorningBriefing::BeginScheduled(const std::string& vietnam_day, int64_t now_us) {
    scheduled_day_ = vietnam_day;
    Start(now_us);
}

void MorningBriefing::TtsStarted(int64_t now_us) {
    if (!Active()) return;
    tts_started_ = true;
    tts_stopped_ = false;
    awaiting_response_ = false;
    last_activity_us_ = now_us;
}

void MorningBriefing::TtsStopped() {
    if (Active() && tts_started_) tts_stopped_ = true;
}

MorningBriefing::Advance MorningBriefing::NextIfReady(
        bool playback_idle, bool channel_open,
        int64_t now_us, std::string& next_text) {
    if (!Active() || !tts_started_ || !tts_stopped_ || !playback_idle)
        return Advance::None;
    if (part_ >= kPartCount) {
        Cancel();
        return Advance::Finished;
    }
    if (!channel_open) {
        Cancel();
        return Advance::ChannelLost;
    }
    // Advance before returning to prevent duplicate continuation when several
    // playback-drained / state events arrive together.
    ++part_;
    tts_started_ = false;
    tts_stopped_ = false;
    awaiting_response_ = true;
    last_activity_us_ = now_us;
    next_text = Followup(part_);
    return Advance::Next;
}

bool MorningBriefing::TimedOut(int64_t now_us) const {
    if (!Active()) return false;
    const int64_t elapsed = now_us - last_activity_us_;
    return elapsed >= 0 && elapsed >
        (awaiting_response_ ? kResponseTimeoutUs : kPlaybackTimeoutUs);
}

void MorningBriefing::Cancel() {
    part_ = 0;
    tts_started_ = false;
    tts_stopped_ = false;
    awaiting_response_ = false;
    last_activity_us_ = 0;
    scheduled_day_.clear();
}

const char* MorningBriefing::InitialRequest() {
    return "Đọc bản tin sáng tự động của gia đình hôm nay. "
        "Đây là đoạn một trên mười hai. Hãy sử dụng cấu hình gia đình và "
        "dữ liệu Master MCP mới nhất. Chỉ đọc đoạn đầu, khoảng một trăm "
        "đến một trăm năm mươi từ, không hỏi có muốn nghe tiếp.";
}
#endif
