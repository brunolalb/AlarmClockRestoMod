#include "alarm_clock.h"

#include <cassert>
#include <filesystem>
#include <iostream>

namespace {

using alarm_clock::AlarmClockController;
using alarm_clock::AlarmTone;
using alarm_clock::BluetoothMode;
using alarm_clock::Clock;
using alarm_clock::RadioContentMode;
using namespace std::chrono_literals;

alarm_clock::TimePoint makeLocalTime(int year, int month, int day, int hour, int minute) {
    std::tm tm {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    return Clock::from_time_t(std::mktime(&tm));
}

void testNextAlarm() {
    AlarmClockController controller;
    controller.alarms().clear();
    controller.alarms().push_back({ "Mon", 7, 30, { true, false, false, false, false, false, false }, AlarmTone::Radio, "Station 1", true, 60 });
    controller.alarms().push_back({ "Tue", 6, 45, { false, true, false, false, false, false, false }, AlarmTone::Buzzer, "", true, 60 });

    const auto monday = makeLocalTime(2026, 5, 11, 7, 0);
    const auto next = controller.nextAlarmAfter(monday);
    assert(next.has_value());
    const auto label = controller.nextAlarmLabel(monday);
    assert(label.find("2026-05-11 07:30") != std::string::npos);
}

void testSnoozeAndDismiss() {
    AlarmClockController controller;
    controller.runtime().alarmActive = true;
    const auto now = makeLocalTime(2026, 5, 12, 6, 0);

    controller.pressSleepButton(1, now);
    assert(!controller.runtime().alarmActive);
    assert(controller.runtime().snoozeUntil.has_value());

    controller.tick(now + 5min);
    assert(controller.runtime().alarmActive);

    controller.pressSleepButton(2, now + 5min);
    assert(!controller.runtime().alarmActive);
    assert(!controller.runtime().snoozeUntil.has_value());
}

void testBluetoothTimeout() {
    AlarmClockController controller;
    const auto now = makeLocalTime(2026, 5, 12, 8, 0);
    controller.pressBluetoothButton(2, now);
    assert(controller.runtime().bluetoothMode == BluetoothMode::PhoneDiscoverable);

    controller.tick(now + 61s);
    assert(controller.runtime().bluetoothMode == BluetoothMode::Off);
}

void testConfigRoundTrip() {
    AlarmClockController controller;
    controller.stations().clear();
    controller.playlists().clear();
    controller.alarms().clear();

    auto config = controller.systemConfig();
    config.network.accessPointSsid = "ClockAP";
    config.network.wifiSsid = "HomeWiFi";
    config.autoDim = false;
    config.vacationMode = true;
    config.radioContentMode = RadioContentMode::Playlists;
    controller.setSystemConfig(config);

    controller.stations().push_back({ "Jazz", "https://example.com/jazz", 1024 });
    controller.playlists().push_back({ "Bedtime", "/sdcard/bedtime.m3u" });
    controller.alarms().push_back({ "Weekend", 9, 15, { false, false, false, false, false, true, true }, AlarmTone::Buzzer, "", true, 45 });

    const auto storage = std::filesystem::temp_directory_path() / "alarm_clock_roundtrip";
    std::filesystem::remove_all(storage);
    controller.save(storage);

    AlarmClockController loaded;
    loaded.load(storage);

    assert(loaded.systemConfig().network.accessPointSsid == "ClockAP");
    assert(loaded.systemConfig().network.wifiSsid == "HomeWiFi");
    assert(!loaded.systemConfig().autoDim);
    assert(loaded.systemConfig().vacationMode);
    assert(loaded.systemConfig().radioContentMode == RadioContentMode::Playlists);
    assert(loaded.stations().size() == 1);
    assert(loaded.playlists().size() == 1);
    assert(loaded.alarms().size() == 1);
}

void testTunerSelection() {
    AlarmClockController controller;
    controller.stations().clear();
    controller.stations().push_back({ "Low", "https://low", 100 });
    controller.stations().push_back({ "High", "https://high", 900 });
    controller.selectTunerFrequency(780);
    assert(controller.runtime().activeSource == "High");
}

} // namespace

int main() {
    testNextAlarm();
    testSnoozeAndDismiss();
    testBluetoothTimeout();
    testConfigRoundTrip();
    testTunerSelection();
    std::cout << "All alarm clock tests passed.\n";
    return 0;
}
