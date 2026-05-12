#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace alarm_clock {

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

enum class AlarmTone {
    Radio,
    Buzzer,
};

enum class DisplayMode {
    Time,
    NextAlarm,
    Sleep,
    IpAddress,
};

enum class RadioContentMode {
    InternetStations,
    Playlists,
};

enum class BluetoothMode {
    Off,
    SpeakerConnect,
    PhoneDiscoverable,
    Connected,
};

struct RadioStation {
    std::string name;
    std::string url;
    int tunerFrequency = 0;
};

struct Playlist {
    std::string name;
    std::string path;
};

struct AlarmConfig {
    std::string name;
    int hour = 7;
    int minute = 0;
    std::array<bool, 7> days { true, true, true, true, true, false, false };
    AlarmTone tone = AlarmTone::Radio;
    std::string sourceName;
    bool enabled = true;
    int sleepMinutes = 60;
};

struct NetworkConfig {
    std::string accessPointSsid = "AlarmClockSetup";
    std::string accessPointPassword = "alarmclock";
    std::string wifiSsid;
    std::string wifiPassword;
};

struct SystemConfig {
    NetworkConfig network;
    bool autoDim = true;
    bool vacationMode = false;
    int defaultSleepMinutes = 60;
    RadioContentMode radioContentMode = RadioContentMode::InternetStations;
    std::string bluetoothSpeakerName;
};

struct RuntimeState {
    DisplayMode displayMode = DisplayMode::Time;
    BluetoothMode bluetoothMode = BluetoothMode::Off;
    bool radioEnabled = false;
    bool sleepModeActive = false;
    bool alarmActive = false;
    bool dimmed = false;
    int volumePercent = 50;
    int tunerFrequency = 0;
    std::string activeSource;
    std::string lastAlarmName;
    std::optional<TimePoint> sleepEndsAt;
    std::optional<TimePoint> snoozeUntil;
    std::optional<TimePoint> bluetoothDeadline;
};

struct StatusSnapshot {
    std::string ipAddress;
    std::string displayValue;
    std::string nextAlarm;
    RuntimeState runtime;
};

class AlarmClockController {
public:
    AlarmClockController();

    void setSystemConfig(const SystemConfig& config);
    const SystemConfig& systemConfig() const;

    std::vector<RadioStation>& stations();
    const std::vector<RadioStation>& stations() const;

    std::vector<Playlist>& playlists();
    const std::vector<Playlist>& playlists() const;

    std::vector<AlarmConfig>& alarms();
    const std::vector<AlarmConfig>& alarms() const;

    RuntimeState& runtime();
    const RuntimeState& runtime() const;

    void load(const std::filesystem::path& storageDir);
    void save(const std::filesystem::path& storageDir) const;

    void tick(TimePoint now);
    void selectTunerFrequency(int frequency);
    void pressSleepButton(int clicks, TimePoint now);
    void pressBluetoothButton(int clicks, TimePoint now);
    void confirmBluetoothConnection();
    void setDisplayMode(DisplayMode mode);
    void setRadioEnabled(bool enabled);
    void setVolumePercent(int percent);
    void startSleepMode(TimePoint now, std::optional<int> minutes = std::nullopt);
    std::optional<TimePoint> nextAlarmAfter(TimePoint now) const;
    std::string nextAlarmLabel(TimePoint now) const;
    StatusSnapshot snapshot(TimePoint now, std::string_view ipAddress) const;
    std::string statusJson(TimePoint now, std::string_view ipAddress) const;
    std::string renderHtml(TimePoint now, std::string_view ipAddress) const;

private:
    void triggerAlarm(const AlarmConfig& alarm, TimePoint now);
    std::string displayValue(TimePoint now, std::string_view ipAddress) const;

    SystemConfig system_;
    RuntimeState runtime_;
    std::vector<RadioStation> stations_;
    std::vector<Playlist> playlists_;
    std::vector<AlarmConfig> alarms_;
    std::optional<std::string> lastTriggeredMinute_;
};

std::string radioContentModeName(RadioContentMode mode);
std::string bluetoothModeName(BluetoothMode mode);
std::string alarmToneName(AlarmTone tone);
std::string dayListLabel(const std::array<bool, 7>& days);
std::array<bool, 7> parseDayList(std::string_view value);

} // namespace alarm_clock
