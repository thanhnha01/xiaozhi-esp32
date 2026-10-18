"""Guard Hi Lily N4R2 pin assignments against regression before ESP-IDF CI."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "main/boards/esp32-s3-zero-tft"


class HiLilyPinoutTests(unittest.TestCase):
    def test_pinout_conflicts_and_audio_clock(self):
        config = (BOARD / "config.h").read_text(encoding="utf-8")
        pins = dict((name, int(pin)) for name, pin in
                    re.findall(r"^#define\\s+(\\w+)\\s+GPIO_NUM_(\\d+)", config, re.M))
        expected = {
            "AUDIO_I2S_SPK_GPIO_MCLK": 1, "AUDIO_I2S_SPK_GPIO_BCLK": 2,
            "AUDIO_I2S_SPK_GPIO_LRCK": 4, "AUDIO_I2S_SPK_GPIO_DOUT": 5,
            "AUDIO_I2S_MIC_GPIO_SCK": 6, "AUDIO_I2S_MIC_GPIO_WS": 7,
            "AUDIO_I2S_MIC_GPIO_DIN": 8, "DISPLAY_RST_PIN": 17,
            "UNUSED_BATTERY_GPIO": 10, "UNUSED_VBUS_GPIO": 16,
        }
        for name, pin in expected.items():
            self.assertEqual(pins.get(name), pin, name)
        device_pins = {name: pin for name, pin in pins.items()
                       if name.startswith(("AUDIO_I2S_", "DISPLAY_", "WAKE_BUTTON_",
                                           "UNUSED_", "BUILTIN_LED_", "BOOT_BUTTON_"))
                       and name not in {"DISPLAY_WIDTH", "DISPLAY_HEIGHT"}}
        self.assertEqual(len(set(device_pins.values())), len(device_pins),
                         f"GPIO collision: {device_pins}")
        board_source = (BOARD / "esp32_s3_zero_tft.cc").read_text(encoding="utf-8")
        codec = (ROOT / "main/audio/codecs/no_audio_codec.cc").read_text(encoding="utf-8")
        self.assertIn("I2S_STD_SLOT_LEFT, AUDIO_I2S_SPK_GPIO_MCLK", board_source)
        self.assertIn(".mclk = spk_mclk", codec)
        self.assertIn("std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;", codec)
        self.assertIn("GPIO_PULLUP_ENABLE", board_source)
        self.assertIn("(1ULL << UNUSED_BATTERY_GPIO) | (1ULL << UNUSED_VBUS_GPIO)", board_source)

    def test_battery_and_vbus_not_active(self):
        config = (BOARD / "config.json").read_text(encoding="utf-8")
        board_source = (BOARD / "esp32_s3_zero_tft.cc").read_text(encoding="utf-8")
        self.assertIn('"CONFIG_HILILY_BATTERY_MONITOR=n"', config)
        self.assertIn('"CONFIG_HILILY_VBUS_SENSE=n"', config)
        self.assertNotIn("battery.ReadLevel(", board_source)
        self.assertNotIn("gpio_get_level(VBUS_SENSE_GPIO)", board_source)
        self.assertIn("return false;", board_source)


if __name__ == "__main__":
    unittest.main()
