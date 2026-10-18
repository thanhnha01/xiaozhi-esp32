# ESP32-S3 Zero N4R2 + TFT 240x240 + PCM5102A + INMP441

Initial **hardware-specific** XiaoZhi board definition for the Waveshare ESP32-S3-Zero
using ESP32-S3FH4R2 (dual-core, **4MB Quad flash / 2MB Quad PSRAM**). This
board has **not yet been hardware-validated**. The LCD controller is assumed to be ST7789.

## Wiring (3.3V GPIO)

| Peripheral | Module pin | ESP32-S3-Zero GPIO |
| --- | --- | --- |
| PCM5102A | SCK / MCLK | 1 (I2S0 MCLK, 6.144 MHz at 24 kHz) |
| PCM5102A | BCK | 2 |
| PCM5102A | LCK / LRCK | 4 |
| PCM5102A | DIN | 5 |
| PCM5102A | VIN / GND | 5V / GND |
| PCM5102A | 3.5mm analog line output | External AUX/amplifier |
| INMP441 | SCK / BCLK | 7 |
| INMP441 | WS / LRCL | 8 |
| INMP441 | SD / DOUT | 9 |
| INMP441 | L/R | GND (left slot) |
| INMP441 | VDD / GND | 3.3V / GND |
| TFT ST7789 | SCL / SCK | 12 |
| TFT ST7789 | SDA / MOSI | 11 |
| TFT ST7789 | DC / A0 | 15 |
| TFT ST7789 | CS | **18 (moved from GPIO9)** |
| TFT ST7789 | RES / RST | **17 (moved from GPIO8)** |
| TFT ST7789 | BL / LED | 13 (PWM) |
| Onboard WS2812 RGB LED | Data | 21 (already connected) |
| Onboard BOOT button | Input | 0 (already connected) |
| External WAKE button | NO pushbutton | GPIO14 to GND (internal pull-up) |
| Unused battery ADC input | No external connection | GPIO10 (input, internal pull-up) |
| Unused charge/VBUS input | No external connection | GPIO16 (input, internal pull-up) |

**Important:** Disconnect any old battery/USB-voltage divider from GPIO10
and GPIO16 before flashing. These inputs now have internal pull-ups enabled,
and neither measures voltage nor charging state. The UI does not report
battery percentage or charging, and does not generate low-battery warnings.
Because INMP441 uses GPIO8 (WS) and GPIO9 (SD), the TFT reset must be
wired to GPIO17 and **the TFT CS wire must be moved from GPIO9 to GPIO18**.
Do not connect TFT CS and INMP441 SD to the same GPIO9.

All modules must share GND. INMP441 uses **3.3V**. PCM5102A is a
**line-level DAC**, not a speaker power amplifier. Connect its 3.5mm
line-out to an amplifier/AUX input, not directly to an unpowered speaker.
Firmware drives PCM5102A SCK/MCLK on GPIO1; ensure your breakout board
actually routes this pin to the DAC. Only the left I2S audio slot is used.

Do not use GPIO19/20 for external peripherals; they serve native USB.
Avoid GPIO3/45/46 (strapping pins) and GPIO33-37 (not exposed on Zero).
GPIO21 is dedicated to the onboard WS2812. **Both BOOT (GPIO0) and external
WAKE (GPIO14)** start/toggle the conversation when clicked. Holding either
button for 2 seconds enters Wi-Fi provisioning after normal boot.
Keep GPIO0 high during reset for normal boot; holding BOOT through reset
enters the ROM download mode. Never wire the external button to GPIO0.

## Display notes

The Hi Lily LCD UI is board-local and keeps default Xiaozhi emojis (no GIF
assets). For all device states, the 240x240 status row reserves space for
Wi-Fi and the battery percentage. Home (weather/lunar), music (12 spectrum
bands, lyric, progress/time) and Wi-Fi setup panels occupy y=34..198, safely
between the top status and the stock bottom subtitle. Long lyrics are
ellipsized inside a 48px block; the 12 spectrum bars are retained. Live radio
shows LIVE elapsed time without a fake total-duration progress bar.
The Wi-Fi setup page wraps the actual AP SSID and browser URL on its own
panel instead of relying on a one-line scrolling subtitle. Critical low
battery alerts are foreground overlays and intentionally cover lower content.
No changes are made to other Xiaozhi boards.


This preliminary configuration assumes a **four-wire SPI ST7789**, 240x240,
SPI mode 0, panel offsets X=0 and Y=0, inversion on. Some 240x240 ST7789
modules need a nonzero vertical offset (often 80) or different inversion,
RGB ordering, or SPI mode; verify the actual LCD controller/pin labels before
soldering and change `config.h` in this board only if necessary.

## Build

Use **ESP-IDF v6.1** (v6.0.1 minimum in this repository):

```sh
python scripts/build.py esp32-s3-zero-tft --name esp32-s3-zero-tft --language vi-VN --wake-word wn9_hilili_tts
idf.py size
```

The preset selects `partitions/v2/4m_hilily.csv`, **Quad PSRAM** instead of the
ESP32-S3 default Octal mode, and the built-in **WakeNet9 Hi Lily** model (`wn9_hilili_tts`),
with speech-processing/AEC and wake-word audio upload off for the
memory-constrained first build. The LVGL pool is reduced to 256KB and its
image cache to 64KB on this board, to leave PSRAM for WakeNet9/AFE.
**The factory firmware and WakeNet model have passed ESP-IDF CI compilation**, but the physical board and charger module have NOT been tested. The Hi Lily 4MB partition layout is **factory-only**:
it has **no dual OTA app slots**, so firmware OTA A/B is not available.
Verify that the compiled app and assets fit in their partitions. A successful
compile does not imply working mic, playback, LCD, Wi-Fi or wake word.

For native USB flashing, enter download mode with the BOOT button when needed,
then run `idf.py -p <port> flash monitor` using your actual serial port.
After power-up test PSRAM initialization, Wi-Fi provisioning, button input,
LCD orientation/backlight, microphone captured PCM, and DAC audio output.

## Audio limitations

The software uses two independent I2S controllers (I2S0 TX / I2S1 RX).
The current `NoAudioCodecSimplex` uses mono samples in the left I2S
slot: connect the **L analog output** of PCM5102A to the amplifier/AUX.
INMP441 L/R must be held low to match left-channel capture.

Acoustic echo cancellation with a playback reference is **not configured**
for this external mic/DAC pair. Do not assume full-duplex AEC will work.

## Hi Lily wake word + pushbuttons

The board enables AFE wake-word support, while the **build command** selects
`wn9_hilili_tts` using `--wake-word` (not a model selector in board config.json). Hi Lily is an actual WakeNet9 model (not merely a changed label).
It requires the ESP-SR 2.4.7 package to provide the symbol and model.
The runtime fetches speech assets from the Hi Lily-specific 4MB layout's 0x130000-byte (1.1875 MiB) assets partition.
If the assets image or firmware exceeds that partition, the **4MB physical
flash is a blocker**, not an option to declare a bigger flash in sdkconfig.
ESP-IDF CI compilation has passed; no physical wake test is claimed.

To switch off wake detection for comparison:
```sh
python scripts/build.py esp32-s3-zero-tft --name esp32-s3-zero-tft --language vi-VN --wake-word disabled
```

## Wi-Fi configuration on phone

The firmware uses `78/esp-wifi-connect` v3.3.1. Hold either button for
about 2 seconds while the device is running (or configure automatically
on first boot). Connect your phone to the `Xiaozhi-XXXX` hotspot.
Open **http://192.168.4.1**. The existing portal automatically fetches
`GET /scan` every 5 seconds, shows selectable detected SSIDs, allows
entering the password, and uses `POST /submit` to connect. Only 2.4GHz
Wi-Fi is supported by ESP32-S3.

The current embedded vendor portal **does not have a dedicated manual
"Quét Wi-Fi" button**; the list already rescans automatically. Changing
the literal portal HTML requires a separately versioned override/fork of
`78/esp-wifi-connect` (rather than editing generated
`managed_components`). This is not falsely claimed as integrated.

## Display goals

See [HILILY_DISPLAY_PLAN.md](HILILY_DISPLAY_PLAN.md) for Worker weather/location,
Vietnamese lunar calendar, mascot emotion assets, music playback lyric/timing,
and battery charging/low-level rules. These features are not yet all wired
end-to-end, especially battery module and synchronized music player.

This revision deliberately disables `CONFIG_HILILY_BATTERY_MONITOR` and
`CONFIG_HILILY_VBUS_SENSE`. GPIO10 and GPIO16 are held as unused
internally pulled-up inputs. Battery percentage, charging bolt and low-battery
warnings are unavailable until a sensor is wired and firmware support
is separately re-enabled.
