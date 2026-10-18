#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "hilily_home_client.h"
#include "hilily_music_player.h"
#include "mcp_server.h"
#include <driver/gpio.h>

#include <esp_log.h>

#define TAG "HiLilyHeadless"

class Esp32S3ZeroTftBoard : public WifiBoard {
private:
    Button boot_button_;
    Button wake_button_;

    void InitializeUnusedSensePins() {
        // GPIO10 is INMP441 data input: I2S owns it, no internal pull-up.
        // Only unused GPIO16 is an input held high with an internal pull-up.
        gpio_config_t cfg = {};
        cfg.pin_bit_mask = 1ULL << UNUSED_VBUS_GPIO;
        cfg.mode = GPIO_MODE_INPUT;
        cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&cfg));
    }

    void InitializeButtons() {
        // Both buttons activate the same conversation path. The external button
        // is active-low, using the internal pull-up configured by Button.
        auto on_short_press = [this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        };
        // A deliberate long press enters Wi-Fi provisioning after normal boot.
        auto on_long_press = [this]() {
            EnterWifiConfigMode();
        };
        boot_button_.OnClick(on_short_press);
        wake_button_.OnClick(on_short_press);
        boot_button_.OnLongPress(on_long_press);
        wake_button_.OnLongPress(on_long_press);
    }

public:
    Esp32S3ZeroTftBoard() : boot_button_(BOOT_BUTTON_GPIO), wake_button_(WAKE_BUTTON_GPIO) {
        InitializeUnusedSensePins();
        InitializeButtons();
        // Keep scheduled Morning Briefing, not the former display/home HTTP polling.
        StartHiLilyMorningScheduleClient();
        auto& music = HiLilyMusicPlayer::GetInstance();
        music.Configure(HILILY_WORKER_BASE_URL);
        // Device-local MCP commands. A gateway may invoke them directly without an LLM.
        auto& mcp = McpServer::GetInstance();
        mcp.AddTool("self.music.play",
            "Play music locally on Hi Lily. ESP32 calls Worker Master directly for MP3 and timed LRC.",
            PropertyList({Property("song", kPropertyTypeString).SetMaxLength(100),
                          Property("artist", kPropertyTypeString, std::string(""))}),
            [](const PropertyList& p) -> ToolResult {
                const auto& song=p["song"].value<std::string>();
                const auto& artist=p["artist"].value<std::string>();
                if (!HiLilyMusicPlayer::GetInstance().Play(song,artist))
                    return std::unexpected("Cannot queue music playback");
                return std::string("Music command queued on device");
            });
        mcp.AddTool("self.radio.play",
            "Play a supported online MP3 radio station on Hi Lily (groove or classic).",
            PropertyList({Property("station", kPropertyTypeString).SetMaxLength(40)}),
            [](const PropertyList& p) -> ReturnValue {
                return HiLilyMusicPlayer::GetInstance().PlayRadio(
                    p["station"].value<std::string>());
            });
        mcp.AddTool("self.music.status", "Get the local Hi Lily online music playback state",
            PropertyList(), [](const PropertyList&) -> ReturnValue {
                return HiLilyMusicPlayer::GetInstance().StatusJson();
            });
        mcp.AddTool("self.music.pause", "Pause local Hi Lily music",
            PropertyList(), [](const PropertyList&) -> ReturnValue {
                return HiLilyMusicPlayer::GetInstance().Pause();
            });
        mcp.AddTool("self.music.resume", "Resume local Hi Lily music",
            PropertyList(), [](const PropertyList&) -> ReturnValue {
                return HiLilyMusicPlayer::GetInstance().Resume();
            });
        mcp.AddTool("self.music.stop", "Stop local Hi Lily music",
            PropertyList(), [](const PropertyList&) -> ReturnValue {
                return HiLilyMusicPlayer::GetInstance().Stop();
            });
    }

    AudioCodec* GetAudioCodec() override {
        // Separate I2S controllers: PCM5102A output and INMP441 input.
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            I2S_STD_SLOT_LEFT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN,
            I2S_STD_SLOT_LEFT, AUDIO_I2S_SPK_GPIO_MCLK);
        return &audio_codec;
    }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        // No cell ADC or charging/VBUS sensor is fitted in this wiring revision.
        // Return unavailable, never fabricate a percentage or charging icon.
        (void)level;
        charging = false;
        discharging = false;
        return false;
    }

    Display* GetDisplay() override {
        // No LVGL/SPI panel is initialized; messages remain in serial logs.
        static NoDisplay display;
        return &display;
    }

    Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }
};

DECLARE_BOARD(Esp32S3ZeroTftBoard);
