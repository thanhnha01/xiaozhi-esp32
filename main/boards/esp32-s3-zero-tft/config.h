#ifndef _ESP32_S3_ZERO_TFT_CONFIG_H_
#define _ESP32_S3_ZERO_TFT_CONFIG_H_

#include <driver/gpio.h>

// Waveshare ESP32-S3-Zero / ESP32-S3FH4R2 (4MB flash + 2MB Quad PSRAM).
// Headless Hi Lily N4R2. GPIO19/20 reserved for native USB, GPIO0 for BOOT.
#define AUDIO_INPUT_SAMPLE_RATE   16000
#define AUDIO_OUTPUT_SAMPLE_RATE  24000

// I2S1 RX: INMP441, connect L/R to GND (left channel).
#define AUDIO_I2S_MIC_GPIO_WS     GPIO_NUM_9
#define AUDIO_I2S_MIC_GPIO_SCK    GPIO_NUM_8
#define AUDIO_I2S_MIC_GPIO_DIN    GPIO_NUM_10

// I2S0 TX: PCM5102A. Only the left DAC channel is populated by
// the current NoAudioCodecSimplex mono output; use L for mono audio.
#define AUDIO_I2S_SPK_GPIO_MCLK   GPIO_NUM_1  // PCM5102A SCK/MCLK (24kHz x 256)
#define AUDIO_I2S_SPK_GPIO_LRCK   GPIO_NUM_5
#define AUDIO_I2S_SPK_GPIO_BCLK   GPIO_NUM_2
#define AUDIO_I2S_SPK_GPIO_DOUT   GPIO_NUM_6

#define BOOT_BUTTON_GPIO         GPIO_NUM_0
#define WAKE_BUTTON_GPIO         GPIO_NUM_14  // External NO pushbutton to GND
#define BUILTIN_LED_GPIO         GPIO_NUM_21  // Onboard WS2812

// Battery/charging sensing is disabled. GPIO10 is now INMP441 SD,
// so never apply a GPIO10 pull-up or initialize ADC there.
// GPIO16 alone remains unused and is held high with an internal pull-up.
#define UNUSED_VBUS_GPIO         GPIO_NUM_16
#define HILILY_WORKER_BASE_URL "https://xiaozhi-master.nguyennha-020201.workers.dev"


#endif  // _ESP32_S3_ZERO_TFT_CONFIG_H_
