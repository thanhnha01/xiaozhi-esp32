"""Native state-machine regression tests for the Hi Lily N4R2 morning briefing.

No ESP-IDF device or extra Python packages needed. The ESP-IDF CI container
already provides the host compiler used for this test.
"""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "main/boards/esp32-s3-zero-tft"
CPP = r'''
#include "morning_briefing.h"
#include <cassert>
#include <string>
int main() {
    MorningBriefing m;
    assert(!m.Active());
    assert(!m.StartedBySpeech("bắt nhạc", 100));
    assert(m.StartedBySpeech("Đọc bản tin sáng", 100));
    assert(!m.StartedBySpeech("Đọc bản tin sáng", 101));
    assert(m.Part() == 1);
    assert(!m.TimedOut(100 + 119LL*1000000));
    assert(m.TimedOut(100 + 121LL*1000000));
    m.Cancel();
    assert(!m.TimedOut(999999999999LL));
    m.BeginScheduled("2026-10-16", 100);
    std::string next;
    // TTS stop without start or drained audio must not advance.
    m.TtsStopped();
    assert(m.NextIfReady(true,true,200,next)==MorningBriefing::Advance::None);
    m.TtsStarted(300);
    assert(!m.TimedOut(300 + 200LL*1000000));
    assert(m.TimedOut(300 + 241LL*1000000));
    m.TtsStopped();
    assert(m.NextIfReady(false,true,300,next)==MorningBriefing::Advance::None);
    assert(m.NextIfReady(true,true,300,next)==MorningBriefing::Advance::Next);
    assert(m.Part()==2 && next.find("đoạn 2")!=std::string::npos);
    // Multiple close-by playback drained events do not produce duplicates.
    assert(m.NextIfReady(true,true,301,next)==MorningBriefing::Advance::None);
    assert(!m.ShouldStopForSpeech("TT_BRIEF_AUTO tiếp tục"));
    assert(m.ShouldStopForSpeech("dừng bản tin"));
    m.Cancel();
    assert(!m.Active());
    m.BeginScheduled("2026-10-16", 400);
    for(int part=2;part<=12;++part){
        m.TtsStarted(part*1000);
        m.TtsStopped();
        assert(m.NextIfReady(true,true,part*1000,next)==MorningBriefing::Advance::Next);
        assert(m.Part()==part);
    }
    m.TtsStarted(20000);
    m.TtsStopped();
    assert(m.NextIfReady(true,true,20001,next)==MorningBriefing::Advance::Finished);
    assert(!m.Active());
    m.BeginScheduled("2026-10-16", 30000);
    m.TtsStarted(30100);
    m.TtsStopped();
    assert(m.NextIfReady(true,false,30200,next)==MorningBriefing::Advance::ChannelLost);
    assert(!m.Active());
    return 0;
}
'''


class MorningBriefingNativeTests(unittest.TestCase):
    def test_state_transitions(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("Host g++ not available")
        with tempfile.TemporaryDirectory() as d:
            temp = Path(d)
            (temp / "sdkconfig.h").write_text(
                "#define CONFIG_BOARD_TYPE_ESP32_S3_ZERO_TFT 1\n",
                encoding="utf-8",
            )
            (temp / "test.cc").write_text(CPP, encoding="utf-8")
            command = [
                compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-I", str(temp), "-I", str(SRC),
                str(SRC / "morning_briefing.cc"), str(temp / "test.cc"),
                "-o", str(temp / "test"),
            ]
            result = subprocess.run(command, text=True, capture_output=True,
                                    timeout=60)
            self.assertEqual(result.returncode, 0, result.stderr)
            run = subprocess.run([str(temp / "test")], text=True,
                                 capture_output=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stderr)


if __name__ == "__main__":
    unittest.main()
