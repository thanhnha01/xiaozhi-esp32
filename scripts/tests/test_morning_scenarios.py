"""Thirty independent native regression scenarios against the production N4R2
MorningBriefing implementation. Compiles the actual source file under test.
"""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "main/boards/esp32-s3-zero-tft"
CASE_NAMES = [
    "initial_idle", "ignore_non_briefing_speech", "recognize_briefing",
    "recognize_capitalized_briefing", "recognize_alternate_briefing",
    "reject_internal_followup_as_new_request", "reject_duplicate_start",
    "stop_command_active", "stop_command_uppercase", "ignore_stop_when_inactive",
    "cancel_deactivates", "cancel_idempotent", "no_advance_before_tts_start",
    "no_advance_before_tts_stop", "no_advance_before_playback_drained",
    "advance_to_second_segment", "avoid_duplicate_drain_advance",
    "response_timeout_below_limit", "response_timeout_above_limit",
    "playback_timeout_below_limit", "playback_timeout_above_limit",
    "inactive_never_times_out", "session_loss_cancels",
    "complete_twelve_segments", "no_thirteenth_segment",
    "scheduled_activation", "manual_restart_after_cancel",
    "ignore_internal_followup_stop", "empty_speech_ignored",
    "user_interruption_prevents_followup",
]
assert len(CASE_NAMES) == 30

HARNESS = r"""
#include "morning_briefing.h"
#include <cstdlib>
#include <iostream>
#include <string>
using Adv = MorningBriefing::Advance;
#define CHECK(c) do { if (!(c)) { \
    std::cerr << "assertion failed: " #c " at " << __LINE__ << '\n'; \
    return 1; } } while(0)
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const int test = std::atoi(argv[1]);
    MorningBriefing m;
    std::string out;
    if(test == 0) { CHECK(!m.Active()); CHECK(m.Part()==0); }
    else if(test == 1) { CHECK(!m.StartedBySpeech("phat nhac",10)); CHECK(!m.Active()); }
    else if(test == 2) { CHECK(m.StartedBySpeech("đọc bản tin sáng",10)); CHECK(m.Part()==1); }
    else if(test == 3) { CHECK(m.StartedBySpeech("Bản tin sáng",10)); }
    else if(test == 4) { CHECK(m.StartedBySpeech("điểm tin hôm nay",10)); }
    else if(test == 5) { CHECK(!m.StartedBySpeech("TT_BRIEF_AUTO bản tin sáng",10)); }
    else if(test == 6) {
        CHECK(m.StartedBySpeech("bản tin sáng",10));
        CHECK(!m.StartedBySpeech("bản tin sáng",11)); CHECK(m.Part()==1);
    }
    else if(test == 7) { m.BeginScheduled("2026-10-16",10); CHECK(m.ShouldStopForSpeech("dừng bản tin")); }
    else if(test == 8) { m.BeginScheduled("2026-10-16",10); CHECK(m.ShouldStopForSpeech("Dừng bản tin")); }
    else if(test == 9) { CHECK(!m.ShouldStopForSpeech("dừng bản tin")); }
    else if(test == 10) { m.BeginScheduled("2026-10-16",10); m.Cancel(); CHECK(!m.Active()); }
    else if(test == 11) { m.Cancel(); m.Cancel(); CHECK(!m.Active()); }
    else if(test == 12) {
        m.BeginScheduled("2026-10-16",10);
        CHECK(m.NextIfReady(true,true,20,out)==Adv::None);
    }
    else if(test == 13) {
        m.BeginScheduled("2026-10-16",10); m.TtsStarted(20);
        CHECK(m.NextIfReady(true,true,30,out)==Adv::None);
    }
    else if(test == 14) {
        m.BeginScheduled("2026-10-16",10); m.TtsStarted(20); m.TtsStopped();
        CHECK(m.NextIfReady(false,true,30,out)==Adv::None);
    }
    else if(test == 15) {
        m.BeginScheduled("2026-10-16",10); m.TtsStarted(20); m.TtsStopped();
        CHECK(m.NextIfReady(true,true,30,out)==Adv::Next);
        CHECK(m.Part()==2); CHECK(out.find("đoạn 2")!=std::string::npos);
    }
    else if(test == 16) {
        m.BeginScheduled("2026-10-16",10); m.TtsStarted(20); m.TtsStopped();
        CHECK(m.NextIfReady(true,true,30,out)==Adv::Next);
        CHECK(m.NextIfReady(true,true,31,out)==Adv::None); CHECK(m.Part()==2);
    }
    else if(test == 17) {
        m.BeginScheduled("2026-10-16",10);
        CHECK(!m.TimedOut(10+119LL*1000000));
    }
    else if(test == 18) {
        m.BeginScheduled("2026-10-16",10);
        CHECK(m.TimedOut(10+121LL*1000000));
    }
    else if(test == 19) {
        m.BeginScheduled("2026-10-16",10); m.TtsStarted(30);
        CHECK(!m.TimedOut(30+239LL*1000000));
    }
    else if(test == 20) {
        m.BeginScheduled("2026-10-16",10); m.TtsStarted(30);
        CHECK(m.TimedOut(30+241LL*1000000));
    }
    else if(test == 21) {
        CHECK(!m.TimedOut(1000000000000LL));
        m.BeginScheduled("2026-10-16",10); m.Cancel();
        CHECK(!m.TimedOut(1000000000000LL));
    }
    else if(test == 22) {
        m.BeginScheduled("2026-10-16",10); m.TtsStarted(20); m.TtsStopped();
        CHECK(m.NextIfReady(true,false,30,out)==Adv::ChannelLost);
        CHECK(!m.Active());
    }
    else if(test == 23) {
        m.BeginScheduled("2026-10-16",10);
        for (int part=2;part<=12;++part) {
            m.TtsStarted(part*1000); m.TtsStopped();
            CHECK(m.NextIfReady(true,true,part*1000+1,out)==Adv::Next);
            CHECK(m.Part()==part);
            CHECK(out.find("đoạn "+std::to_string(part))!=std::string::npos);
        }
        m.TtsStarted(50000); m.TtsStopped();
        CHECK(m.NextIfReady(true,true,50001,out)==Adv::Finished);
        CHECK(!m.Active());
    }
    else if(test == 24) {
        m.BeginScheduled("2026-10-16",10);
        for(int part=2;part<=12;++part) {
            m.TtsStarted(part*1000);m.TtsStopped();
            CHECK(m.NextIfReady(true,true,part*1000+1,out)==Adv::Next);
        }
        m.TtsStarted(30000); m.TtsStopped();
        CHECK(m.NextIfReady(true,true,30001,out)==Adv::Finished);
        CHECK(m.NextIfReady(true,true,30002,out)==Adv::None);
    }
    else if(test == 25) { m.BeginScheduled("2026-10-16",10); CHECK(m.Active()); CHECK(m.Part()==1); }
    else if(test == 26) {
        m.BeginScheduled("2026-10-16",10); m.Cancel();
        CHECK(m.StartedBySpeech("bản tin sáng",20)); CHECK(m.Part()==1);
    }
    else if(test == 27) {
        m.BeginScheduled("2026-10-16",10);
        CHECK(!m.ShouldStopForSpeech("TT_BRIEF_AUTO dừng bản tin"));
    }
    else if(test == 28) { CHECK(!m.StartedBySpeech("",10)); CHECK(!m.Active()); }
    else if(test == 29) {
        m.BeginScheduled("2026-10-16",10); m.TtsStarted(20); m.TtsStopped();
        CHECK(m.ShouldStopForSpeech("dừng bản tin")); m.Cancel();
        CHECK(m.NextIfReady(true,true,30,out)==Adv::None);
        CHECK(!m.Active());
    }
    else return 3;
    return 0;
}
"""

class MorningUserScenarios(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("g++")
        if not compiler:
            raise unittest.SkipTest("g++ not present")
        cls.temp = tempfile.TemporaryDirectory()
        p = Path(cls.temp.name)
        (p / "sdkconfig.h").write_text(
            "#define CONFIG_BOARD_TYPE_ESP32_S3_ZERO_TFT 1\n", encoding="utf8"
        )
        (p / "cases.cc").write_text(HARNESS, encoding="utf8")
        cls.program = str(p / "cases")
        cmd = [
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=undefined", "-fno-omit-frame-pointer",
            "-I", str(p), "-I", str(SRC),
            str(SRC / "morning_briefing.cc"), str(p / "cases.cc"),
            "-o", cls.program,
        ]
        compiled = subprocess.run(cmd, text=True, capture_output=True, timeout=90)
        if compiled.returncode:
            raise AssertionError("native harness failed:\n" + compiled.stderr)

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "temp"):
            cls.temp.cleanup()


def _make_test(i):
    def test_case(self):
        result = subprocess.run(
            [self.program, str(i)], capture_output=True, text=True, timeout=10
        )
        self.assertEqual(result.returncode, 0, result.stderr)
    return test_case


for _index, _name in enumerate(CASE_NAMES):
    setattr(MorningUserScenarios, "test_%02d_%s" % (_index, _name), _make_test(_index))

if __name__ == "__main__":
    unittest.main()
