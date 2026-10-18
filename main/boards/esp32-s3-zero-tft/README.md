# Hi Lily — ESP32-S3 Zero N4R2, headless voice speaker

This board runs without any TFT/OLED/LCD. The build target remains
`esp32-s3-zero-tft` for compatibility with the existing CI and release workflow,
but **the firmware does not initialize a screen or a SPI display bus**.

## Wiring (current soldered layout)

| Device | Module pin | ESP32-S3 GPIO |
| --- | --- | --- |
| PCM5102A DAC | SCK/MCLK | GPIO1 (I2S0 MCLK) |
| PCM5102A DAC | BCK | GPIO2 |
| PCM5102A DAC | LRCK/LCK | GPIO5 |
| PCM5102A DAC | DIN | GPIO6 |
| PCM5102A DAC | VIN / GND | 5V / GND |
| PCM5102A DAC | 3.5 mm line out | Input of powered external amplifier |
| INMP441 microphone | SCK | GPIO8 |
| INMP441 microphone | WS | GPIO9 |
| INMP441 microphone | SD | GPIO10 |
| INMP441 microphone | L/R | GND (left I2S slot) |
| INMP441 microphone | VDD / GND | 3.3V / GND |
| External NO wake button | Pin 1 / Pin 2 | GPIO14 / GND (internal pull-up) |
| Onboard BOOT button | Built in | GPIO0 |
| Onboard WS2812 | Built in | GPIO21 |
| Unused charger VBUS | Not connected | GPIO16 (internal pull-up) |

GPIO3, GPIO4, GPIO7, GPIO11–13, GPIO15, GPIO17 and GPIO18 are not
allocated by this board preset. **GPIO13 is free, but WAKE is still on
GPIO14** until the wiring and code are explicitly changed.

No TFT wiring is needed; previous TFT BL/CS/RST/DC/MOSI/SCK pins are unassigned.
Do not leave display modules connected to the board. GPIO19/20 are reserved
for native USB; avoid GPIO3/45/46 strapping pins. Share GND between audio modules.
PCM5102A analog outputs are line level, not amplified speaker outputs.

## Retained functions

- WakeNet9 `wn9_hilili_tts`, external WAKE and BOOT buttons.
- Wi-Fi configuration via AP and web captive portal (no screen needed).
- XiaoZhi conversation/voice playback, Worker MCP, online music and radio.
- Scheduled Morning Briefing: the Worker schedule poller is retained;
  the screen weather/lunar polling and task UI notifications are removed.
- Simple `NoDisplay` logging backend; no ST7789, SPI2 LCD, LVGL screen updates,
  subtitle UI, GIFs, song lyrics or visualizer.
- Battery ADC and charging detection remain disabled; GPIO10 is MIC SD, and
  only unused GPIO16 has an internal pull-up.

## Build and Release

```sh
python3 scripts/build.py esp32-s3-zero-tft --name esp32-s3-zero-tft --language vi-VN --wake-word wn9_hilili_tts
```

The 4MB flash + 2MB Quad PSRAM target and `partitions/v2/4m_hilily.csv`
factory-only layout are unchanged. The GitHub Actions Hi Lily workflow builds
and creates a **prerelease only after the binary and assets size checks pass**.
There are **no OTA A/B slots**. Flash the full merged image over USB.
Hardware testing is still required, including I2S MCLK, microphone, Wi-Fi,
wake word and speaker playback.
