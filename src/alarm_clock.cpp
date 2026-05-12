#include "alarm_clock.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace alarm_clock {
namespace {

constexpr std::array<std::string_view, 7> kDayNames { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
constexpr auto kSnoozeDuration = std::chrono::minutes(5);
constexpr auto kBluetoothTimeout = std::chrono::minutes(1);

std::tm localTm(TimePoint when) {
    const auto raw = Clock::to_time_t(when);
    std::tm result {};
#if defined(_WIN32)
    localtime_s(&result, &raw);
#else
    localtime_r(&raw, &result);
#endif
    return result;
}

int mondayBasedIndex(const std::tm& tm) {
    return (tm.tm_wday + 6) % 7;
}

std::string formatTime(const std::tm& tm) {
    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%d %H:%M");
    return stream.str();
}

std::string formatHm(int hour, int minute) {
    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(2) << hour << ":" << std::setw(2) << minute;
    return stream.str();
}

std::string jsonEscape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

std::string htmlEscape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&#39;";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

std::string trim(std::string value) {
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool isAlarmDue(const AlarmConfig& alarm, TimePoint now, const RuntimeState& runtime, const std::optional<std::string>& lastTriggeredMinute) {
    if (!alarm.enabled || runtime.snoozeUntil.has_value()) {
        return false;
    }

    const std::tm tm = localTm(now);
    if (!alarm.days[static_cast<std::size_t>(mondayBasedIndex(tm))]) {
        return false;
    }

    if (tm.tm_hour != alarm.hour || tm.tm_min != alarm.minute) {
        return false;
    }

    const auto key = alarm.name + "@" + formatTime(tm);
    return !lastTriggeredMinute.has_value() || lastTriggeredMinute.value() != key;
}

std::string daysToBits(const std::array<bool, 7>& days) {
    std::string bits;
    bits.reserve(days.size());
    for (const bool value : days) {
        bits += value ? '1' : '0';
    }
    return bits;
}

std::string optionalTimeLabel(const std::optional<TimePoint>& value) {
    if (!value.has_value()) {
        return "-";
    }

    const std::tm tm = localTm(value.value());
    return formatTime(tm);
}

} // namespace

AlarmClockController::AlarmClockController() {
    stations_.push_back({ "Station 1", "https://example.com/radio", 1200 });
    playlists_.push_back({ "Wake Up Mix", "/sdcard/playlists/wakeup.m3u" });
    alarms_.push_back({ "Weekday", 7, 0, { true, true, true, true, true, false, false }, AlarmTone::Radio, "Station 1", true, 60 });
}

void AlarmClockController::setSystemConfig(const SystemConfig& config) {
    system_ = config;
    runtime_.dimmed = config.autoDim;
}

const SystemConfig& AlarmClockController::systemConfig() const {
    return system_;
}

std::vector<RadioStation>& AlarmClockController::stations() {
    return stations_;
}

const std::vector<RadioStation>& AlarmClockController::stations() const {
    return stations_;
}

std::vector<Playlist>& AlarmClockController::playlists() {
    return playlists_;
}

const std::vector<Playlist>& AlarmClockController::playlists() const {
    return playlists_;
}

std::vector<AlarmConfig>& AlarmClockController::alarms() {
    return alarms_;
}

const std::vector<AlarmConfig>& AlarmClockController::alarms() const {
    return alarms_;
}

RuntimeState& AlarmClockController::runtime() {
    return runtime_;
}

const RuntimeState& AlarmClockController::runtime() const {
    return runtime_;
}

void AlarmClockController::load(const std::filesystem::path& storageDir) {
    std::filesystem::create_directories(storageDir);

    if (std::ifstream systemFile { storageDir / "system.cfg" }) {
        SystemConfig loaded = system_;
        std::string modeToken;
        int autoDim = 1;
        int vacation = 0;
        systemFile >> std::quoted(loaded.network.accessPointSsid)
                   >> std::quoted(loaded.network.accessPointPassword)
                   >> std::quoted(loaded.network.wifiSsid)
                   >> std::quoted(loaded.network.wifiPassword)
                   >> autoDim
                   >> vacation
                   >> loaded.defaultSleepMinutes
                   >> std::quoted(modeToken)
                   >> std::quoted(loaded.bluetoothSpeakerName);
        loaded.autoDim = autoDim != 0;
        loaded.vacationMode = vacation != 0;
        loaded.radioContentMode = modeToken == "playlists" ? RadioContentMode::Playlists : RadioContentMode::InternetStations;
        setSystemConfig(loaded);
    }

    if (std::ifstream stationsFile { storageDir / "stations.cfg" }) {
        stations_.clear();
        RadioStation station;
        while (stationsFile >> std::quoted(station.name) >> std::quoted(station.url) >> station.tunerFrequency) {
            stations_.push_back(station);
        }
    }

    if (std::ifstream playlistsFile { storageDir / "playlists.cfg" }) {
        playlists_.clear();
        Playlist playlist;
        while (playlistsFile >> std::quoted(playlist.name) >> std::quoted(playlist.path)) {
            playlists_.push_back(playlist);
        }
    }

    if (std::ifstream alarmsFile { storageDir / "alarms.cfg" }) {
        alarms_.clear();
        AlarmConfig alarm;
        std::string daysBits;
        std::string toneToken;
        int enabled = 1;
        while (alarmsFile >> std::quoted(alarm.name)
                          >> alarm.hour
                          >> alarm.minute
                          >> std::quoted(daysBits)
                          >> std::quoted(toneToken)
                          >> std::quoted(alarm.sourceName)
                          >> enabled
                          >> alarm.sleepMinutes) {
            alarm.days = parseDayList(daysBits);
            alarm.tone = toneToken == "buzzer" ? AlarmTone::Buzzer : AlarmTone::Radio;
            alarm.enabled = enabled != 0;
            alarms_.push_back(alarm);
        }
    }
}

void AlarmClockController::save(const std::filesystem::path& storageDir) const {
    std::filesystem::create_directories(storageDir);

    {
        std::ofstream systemFile(storageDir / "system.cfg", std::ios::trunc);
        systemFile << std::quoted(system_.network.accessPointSsid) << ' '
                   << std::quoted(system_.network.accessPointPassword) << ' '
                   << std::quoted(system_.network.wifiSsid) << ' '
                   << std::quoted(system_.network.wifiPassword) << ' '
                   << (system_.autoDim ? 1 : 0) << ' '
                   << (system_.vacationMode ? 1 : 0) << ' '
                   << system_.defaultSleepMinutes << ' '
                   << std::quoted(system_.radioContentMode == RadioContentMode::Playlists ? "playlists" : "stations") << ' '
                   << std::quoted(system_.bluetoothSpeakerName) << '\n';
    }

    {
        std::ofstream stationsFile(storageDir / "stations.cfg", std::ios::trunc);
        for (const auto& station : stations_) {
            stationsFile << std::quoted(station.name) << ' '
                         << std::quoted(station.url) << ' '
                         << station.tunerFrequency << '\n';
        }
    }

    {
        std::ofstream playlistsFile(storageDir / "playlists.cfg", std::ios::trunc);
        for (const auto& playlist : playlists_) {
            playlistsFile << std::quoted(playlist.name) << ' '
                          << std::quoted(playlist.path) << '\n';
        }
    }

    {
        std::ofstream alarmsFile(storageDir / "alarms.cfg", std::ios::trunc);
        for (const auto& alarm : alarms_) {
            alarmsFile << std::quoted(alarm.name) << ' '
                       << alarm.hour << ' '
                       << alarm.minute << ' '
                       << std::quoted(daysToBits(alarm.days)) << ' '
                       << std::quoted(alarm.tone == AlarmTone::Buzzer ? "buzzer" : "radio") << ' '
                       << std::quoted(alarm.sourceName) << ' '
                       << (alarm.enabled ? 1 : 0) << ' '
                       << alarm.sleepMinutes << '\n';
        }
    }
}

void AlarmClockController::tick(TimePoint now) {
    if (runtime_.bluetoothDeadline.has_value() && now >= runtime_.bluetoothDeadline.value() && runtime_.bluetoothMode != BluetoothMode::Connected) {
        runtime_.bluetoothMode = BluetoothMode::Off;
        runtime_.bluetoothDeadline.reset();
    }

    if (runtime_.sleepEndsAt.has_value() && now >= runtime_.sleepEndsAt.value()) {
        runtime_.sleepEndsAt.reset();
        runtime_.sleepModeActive = false;
        runtime_.radioEnabled = false;
    }

    if (runtime_.snoozeUntil.has_value() && now >= runtime_.snoozeUntil.value()) {
        runtime_.snoozeUntil.reset();
        runtime_.alarmActive = true;
    }

    if (system_.vacationMode || runtime_.alarmActive) {
        return;
    }

    for (const auto& alarm : alarms_) {
        if (isAlarmDue(alarm, now, runtime_, lastTriggeredMinute_)) {
            triggerAlarm(alarm, now);
            break;
        }
    }
}

void AlarmClockController::selectTunerFrequency(int frequency) {
    runtime_.tunerFrequency = frequency;
    if (stations_.empty()) {
        runtime_.activeSource.clear();
        return;
    }

    const auto best = std::min_element(stations_.begin(), stations_.end(), [frequency](const auto& left, const auto& right) {
        return std::abs(left.tunerFrequency - frequency) < std::abs(right.tunerFrequency - frequency);
    });

    runtime_.activeSource = best->name;
}

void AlarmClockController::pressSleepButton(int clicks, TimePoint now) {
    if (runtime_.alarmActive) {
        if (clicks <= 1) {
            runtime_.alarmActive = false;
            runtime_.snoozeUntil = now + kSnoozeDuration;
        } else {
            runtime_.alarmActive = false;
            runtime_.snoozeUntil.reset();
        }
        return;
    }

    if (runtime_.sleepModeActive) {
        runtime_.sleepModeActive = false;
        runtime_.radioEnabled = false;
        runtime_.sleepEndsAt.reset();
    }
}

void AlarmClockController::pressBluetoothButton(int clicks, TimePoint now) {
    runtime_.bluetoothDeadline = now + kBluetoothTimeout;
    if (clicks <= 1) {
        runtime_.bluetoothMode = BluetoothMode::SpeakerConnect;
        return;
    }

    runtime_.bluetoothMode = BluetoothMode::PhoneDiscoverable;
}

void AlarmClockController::confirmBluetoothConnection() {
    runtime_.bluetoothMode = BluetoothMode::Connected;
    runtime_.bluetoothDeadline.reset();
}

void AlarmClockController::setDisplayMode(DisplayMode mode) {
    runtime_.displayMode = mode;
}

void AlarmClockController::setRadioEnabled(bool enabled) {
    runtime_.radioEnabled = enabled;
}

void AlarmClockController::setVolumePercent(int percent) {
    runtime_.volumePercent = std::clamp(percent, 0, 100);
}

void AlarmClockController::startSleepMode(TimePoint now, std::optional<int> minutes) {
    runtime_.sleepModeActive = true;
    runtime_.radioEnabled = true;
    runtime_.displayMode = DisplayMode::Sleep;
    runtime_.sleepEndsAt = now + std::chrono::minutes(minutes.value_or(system_.defaultSleepMinutes));
}

std::optional<TimePoint> AlarmClockController::nextAlarmAfter(TimePoint now) const {
    if (system_.vacationMode || alarms_.empty()) {
        return std::nullopt;
    }

    std::optional<TimePoint> best;
    for (int offset = 0; offset < 8; ++offset) {
        const auto candidateDay = now + std::chrono::hours(24 * offset);
        const std::tm candidateTm = localTm(candidateDay);
        for (const auto& alarm : alarms_) {
            if (!alarm.enabled || !alarm.days[static_cast<std::size_t>(mondayBasedIndex(candidateTm))]) {
                continue;
            }

            std::tm scheduled = candidateTm;
            scheduled.tm_hour = alarm.hour;
            scheduled.tm_min = alarm.minute;
            scheduled.tm_sec = 0;

            const auto scheduledTime = Clock::from_time_t(std::mktime(&scheduled));
            if (scheduledTime < now) {
                continue;
            }

            if (!best.has_value() || scheduledTime < best.value()) {
                best = scheduledTime;
            }
        }
    }
    return best;
}

std::string AlarmClockController::nextAlarmLabel(TimePoint now) const {
    const auto next = nextAlarmAfter(now);
    if (!next.has_value()) {
        return "None";
    }
    const std::tm tm = localTm(next.value());
    return formatTime(tm);
}

StatusSnapshot AlarmClockController::snapshot(TimePoint now, std::string_view ipAddress) const {
    return { std::string(ipAddress), displayValue(now, ipAddress), nextAlarmLabel(now), runtime_ };
}

std::string AlarmClockController::statusJson(TimePoint now, std::string_view ipAddress) const {
    std::ostringstream json;
    json << "{"
         << "\"ipAddress\":\"" << jsonEscape(ipAddress) << "\","
         << "\"displayMode\":\"" << jsonEscape(displayValue(now, ipAddress)) << "\","
         << "\"nextAlarm\":\"" << jsonEscape(nextAlarmLabel(now)) << "\","
         << "\"autoDim\":" << (system_.autoDim ? "true" : "false") << ","
         << "\"vacationMode\":" << (system_.vacationMode ? "true" : "false") << ","
         << "\"radioContentMode\":\"" << jsonEscape(radioContentModeName(system_.radioContentMode)) << "\","
         << "\"bluetoothMode\":\"" << jsonEscape(bluetoothModeName(runtime_.bluetoothMode)) << "\","
         << "\"radioEnabled\":" << (runtime_.radioEnabled ? "true" : "false") << ","
         << "\"sleepModeActive\":" << (runtime_.sleepModeActive ? "true" : "false") << ","
         << "\"alarmActive\":" << (runtime_.alarmActive ? "true" : "false") << ","
         << "\"volumePercent\":" << runtime_.volumePercent << ","
         << "\"activeSource\":\"" << jsonEscape(runtime_.activeSource) << "\","
         << "\"sleepEndsAt\":\"" << jsonEscape(optionalTimeLabel(runtime_.sleepEndsAt)) << "\""
         << "}";
    return json.str();
}

std::string AlarmClockController::renderHtml(TimePoint now, std::string_view ipAddress) const {
    std::ostringstream html;
    html << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Alarm Clock</title>"
         << "<style>body{font-family:sans-serif;max-width:960px;margin:2rem auto;padding:0 1rem;line-height:1.4}"
         << "table{border-collapse:collapse;width:100%;margin-bottom:1rem}th,td{border:1px solid #ccc;padding:.5rem;text-align:left}"
         << "form{border:1px solid #ccc;padding:1rem;margin-bottom:1rem}label{display:block;margin:.4rem 0}"
         << "input,select{width:100%;padding:.4rem}.actions form{display:inline-block;border:none;padding:0;margin-right:.5rem}</style></head><body>";
    html << "<h1>Alarm Clock control panel</h1>";
    html << "<p><strong>IP:</strong> " << htmlEscape(ipAddress) << " | <strong>Display:</strong> " << htmlEscape(displayValue(now, ipAddress))
         << " | <strong>Next alarm:</strong> " << htmlEscape(nextAlarmLabel(now)) << "</p>";
    html << "<p><strong>Bluetooth:</strong> " << htmlEscape(bluetoothModeName(runtime_.bluetoothMode))
         << " | <strong>Source mode:</strong> " << htmlEscape(radioContentModeName(system_.radioContentMode))
         << " | <strong>Sleep ends:</strong> " << htmlEscape(optionalTimeLabel(runtime_.sleepEndsAt)) << "</p>";

    html << "<div class=\"actions\">"
         << "<form method=\"post\" action=\"/action/radio/on\"><button>Radio on</button></form>"
         << "<form method=\"post\" action=\"/action/radio/off\"><button>Radio off</button></form>"
         << "<form method=\"post\" action=\"/action/sleep/start\"><button>Start sleep</button></form>"
         << "<form method=\"post\" action=\"/action/display/ip\"><button>Show IP</button></form>"
         << "<form method=\"post\" action=\"/action/bluetooth/speaker\"><button>Bluetooth speaker</button></form>"
         << "<form method=\"post\" action=\"/action/bluetooth/phone\"><button>Bluetooth phone</button></form>"
         << "</div>";

    html << "<h2>System</h2><form method=\"post\" action=\"/config/system\">"
         << "<label>Access point SSID<input name=\"ap_ssid\" value=\"" << htmlEscape(system_.network.accessPointSsid) << "\"></label>"
         << "<label>Access point password<input name=\"ap_password\" value=\"" << htmlEscape(system_.network.accessPointPassword) << "\"></label>"
         << "<label>Wi-Fi SSID<input name=\"wifi_ssid\" value=\"" << htmlEscape(system_.network.wifiSsid) << "\"></label>"
         << "<label>Wi-Fi password<input name=\"wifi_password\" value=\"" << htmlEscape(system_.network.wifiPassword) << "\"></label>"
         << "<label>Default sleep minutes<input name=\"sleep_minutes\" type=\"number\" min=\"1\" value=\"" << system_.defaultSleepMinutes << "\"></label>"
         << "<label>Auto dim<select name=\"auto_dim\"><option value=\"1\"" << (system_.autoDim ? " selected" : "") << ">On</option><option value=\"0\"" << (!system_.autoDim ? " selected" : "") << ">Off</option></select></label>"
         << "<label>Vacation mode<select name=\"vacation\"><option value=\"0\"" << (!system_.vacationMode ? " selected" : "") << ">Off</option><option value=\"1\"" << (system_.vacationMode ? " selected" : "") << ">On</option></select></label>"
         << "<label>Radio content<select name=\"content_mode\"><option value=\"stations\"" << (system_.radioContentMode == RadioContentMode::InternetStations ? " selected" : "") << ">Internet radios</option><option value=\"playlists\"" << (system_.radioContentMode == RadioContentMode::Playlists ? " selected" : "") << ">Playlists</option></select></label>"
         << "<label>Bluetooth speaker name<input name=\"speaker_name\" value=\"" << htmlEscape(system_.bluetoothSpeakerName) << "\"></label>"
         << "<button>Save system config</button></form>";

    html << "<h2>Radio stations</h2><table><tr><th>Name</th><th>URL</th><th>Tuner value</th></tr>";
    for (const auto& station : stations_) {
        html << "<tr><td>" << htmlEscape(station.name) << "</td><td>" << htmlEscape(station.url) << "</td><td>" << station.tunerFrequency << "</td></tr>";
    }
    html << "</table><form method=\"post\" action=\"/config/station\">"
         << "<label>Name<input name=\"name\"></label>"
         << "<label>Stream URL<input name=\"url\"></label>"
         << "<label>Tuner value<input name=\"frequency\" type=\"number\"></label>"
         << "<button>Add station</button></form>";

    html << "<h2>Playlists</h2><table><tr><th>Name</th><th>Path</th></tr>";
    for (const auto& playlist : playlists_) {
        html << "<tr><td>" << htmlEscape(playlist.name) << "</td><td>" << htmlEscape(playlist.path) << "</td></tr>";
    }
    html << "</table><form method=\"post\" action=\"/config/playlist\">"
         << "<label>Name<input name=\"name\"></label>"
         << "<label>microSD path<input name=\"path\"></label>"
         << "<button>Add playlist</button></form>";

    html << "<h2>Alarms</h2><table><tr><th>Name</th><th>Time</th><th>Days</th><th>Type</th><th>Source</th><th>Enabled</th><th>Sleep</th></tr>";
    for (const auto& alarm : alarms_) {
        html << "<tr><td>" << htmlEscape(alarm.name) << "</td><td>" << htmlEscape(formatHm(alarm.hour, alarm.minute)) << "</td><td>"
             << htmlEscape(dayListLabel(alarm.days)) << "</td><td>" << htmlEscape(alarmToneName(alarm.tone))
             << "</td><td>" << htmlEscape(alarm.sourceName) << "</td><td>" << (alarm.enabled ? "Yes" : "No")
             << "</td><td>" << alarm.sleepMinutes << " min</td></tr>";
    }
    html << "</table><form method=\"post\" action=\"/config/alarm\">"
         << "<label>Name<input name=\"name\"></label>"
         << "<label>Time HH:MM<input name=\"time\" value=\"07:00\"></label>"
         << "<label>Days (Mon,Tue,Wed,Thu,Fri,Sat,Sun)<input name=\"days\" value=\"Mon,Tue,Wed,Thu,Fri\"></label>"
         << "<label>Alarm type<select name=\"tone\"><option value=\"radio\">Radio</option><option value=\"buzzer\">Buzzer</option></select></label>"
         << "<label>Source name<input name=\"source\"></label>"
         << "<label>Enabled<select name=\"enabled\"><option value=\"1\">Yes</option><option value=\"0\">No</option></select></label>"
         << "<label>Sleep minutes<input name=\"sleep_minutes\" type=\"number\" min=\"1\" value=\"60\"></label>"
         << "<button>Add alarm</button></form>";

    html << "<p>Configuration files are stored in the storage directory to mirror the microSD-based setup expected on the ESP32-S3 target.</p>";
    html << "</body></html>";
    return html.str();
}

void AlarmClockController::triggerAlarm(const AlarmConfig& alarm, TimePoint now) {
    runtime_.alarmActive = true;
    runtime_.radioEnabled = alarm.tone == AlarmTone::Radio;
    runtime_.lastAlarmName = alarm.name;
    runtime_.activeSource = alarm.sourceName;
    lastTriggeredMinute_ = alarm.name + "@" + formatTime(localTm(now));
}

std::string AlarmClockController::displayValue(TimePoint now, std::string_view ipAddress) const {
    const std::tm tm = localTm(now);
    switch (runtime_.displayMode) {
    case DisplayMode::Time:
        return formatHm(tm.tm_hour, tm.tm_min);
    case DisplayMode::NextAlarm:
        return nextAlarmLabel(now);
    case DisplayMode::Sleep:
        return runtime_.sleepEndsAt.has_value() ? optionalTimeLabel(runtime_.sleepEndsAt) : "Sleep off";
    case DisplayMode::IpAddress:
        return std::string(ipAddress);
    default:
        return formatHm(tm.tm_hour, tm.tm_min);
    }
}

std::string radioContentModeName(RadioContentMode mode) {
    return mode == RadioContentMode::Playlists ? "Playlists" : "Internet stations";
}

std::string bluetoothModeName(BluetoothMode mode) {
    switch (mode) {
    case BluetoothMode::Off:
        return "Off";
    case BluetoothMode::SpeakerConnect:
        return "Connecting speaker";
    case BluetoothMode::PhoneDiscoverable:
        return "Phone pairing";
    case BluetoothMode::Connected:
        return "Connected";
    }
    return "Unknown";
}

std::string alarmToneName(AlarmTone tone) {
    return tone == AlarmTone::Buzzer ? "Buzzer" : "Radio";
}

std::string dayListLabel(const std::array<bool, 7>& days) {
    std::ostringstream stream;
    bool first = true;
    for (std::size_t index = 0; index < days.size(); ++index) {
        if (!days[index]) {
            continue;
        }
        if (!first) {
            stream << ", ";
        }
        stream << kDayNames[index];
        first = false;
    }
    return first ? "None" : stream.str();
}

std::array<bool, 7> parseDayList(std::string_view value) {
    std::array<bool, 7> days { false, false, false, false, false, false, false };

    const std::string raw(value);
    if (raw.size() == 7 && std::all_of(raw.begin(), raw.end(), [](char ch) { return ch == '0' || ch == '1'; })) {
        for (std::size_t index = 0; index < 7; ++index) {
            days[index] = raw[index] == '1';
        }
        return days;
    }

    std::stringstream stream(raw);
    std::string token;
    while (std::getline(stream, token, ',')) {
        token = trim(token);
        for (std::size_t index = 0; index < kDayNames.size(); ++index) {
            if (token == kDayNames[index]) {
                days[index] = true;
            }
        }
    }
    return days;
}

} // namespace alarm_clock
