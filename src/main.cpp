#include "alarm_clock.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace {

using alarm_clock::AlarmClockController;
using alarm_clock::AlarmConfig;
using alarm_clock::AlarmTone;
using alarm_clock::DisplayMode;
using alarm_clock::Playlist;
using alarm_clock::RadioContentMode;
using alarm_clock::RadioStation;

std::string urlDecode(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+' ) {
            decoded += ' ';
            continue;
        }

        if (value[index] == '%' && index + 2 < value.size()) {
            const auto hex = std::string(value.substr(index + 1, 2));
            decoded += static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            index += 2;
            continue;
        }

        decoded += value[index];
    }
    return decoded;
}

std::map<std::string, std::string> parseForm(std::string_view body) {
    std::map<std::string, std::string> fields;
    std::stringstream stream { std::string(body) };
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        const auto split = pair.find('=');
        const auto key = urlDecode(pair.substr(0, split));
        const auto value = split == std::string::npos ? std::string() : urlDecode(pair.substr(split + 1));
        fields[key] = value;
    }
    return fields;
}

std::string readRequest(int client) {
    std::string request;
    char buffer[4096];
    while (true) {
        const auto bytes = recv(client, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            break;
        }
        request.append(buffer, static_cast<std::size_t>(bytes));

        const auto headerEnd = request.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            continue;
        }

        std::size_t contentLength = 0;
        std::istringstream headerStream(request.substr(0, headerEnd));
        std::string line;
        while (std::getline(headerStream, line)) {
            if (const auto pos = line.find("Content-Length:"); pos != std::string::npos) {
                contentLength = static_cast<std::size_t>(std::stoul(line.substr(pos + 15)));
            }
        }

        if (request.size() >= headerEnd + 4 + contentLength) {
            break;
        }
    }
    return request;
}

void sendResponse(int client, std::string_view status, std::string_view contentType, std::string_view body, std::string_view extraHeaders = {}) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n";
    if (!extraHeaders.empty()) {
        response << extraHeaders;
    }
    response << "\r\n" << body;
    const auto text = response.str();
    send(client, text.data(), text.size(), 0);
}

void redirectHome(int client) {
    sendResponse(client, "303 See Other", "text/plain", "Updated", "Location: /\r\n");
}

int parseIntFromFields(const std::map<std::string, std::string>& fields, const std::string& key, int fallback) {
    if (const auto it = fields.find(key); it != fields.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
        }
    }
    return fallback;
}

void applySystemConfig(AlarmClockController& controller, const std::map<std::string, std::string>& fields) {
    auto config = controller.systemConfig();
    if (const auto it = fields.find("ap_ssid"); it != fields.end()) {
        config.network.accessPointSsid = it->second;
    }
    if (const auto it = fields.find("ap_password"); it != fields.end()) {
        config.network.accessPointPassword = it->second;
    }
    if (const auto it = fields.find("wifi_ssid"); it != fields.end()) {
        config.network.wifiSsid = it->second;
    }
    if (const auto it = fields.find("wifi_password"); it != fields.end()) {
        config.network.wifiPassword = it->second;
    }
    if (const auto it = fields.find("speaker_name"); it != fields.end()) {
        config.bluetoothSpeakerName = it->second;
    }
    config.autoDim = parseIntFromFields(fields, "auto_dim", config.autoDim ? 1 : 0) != 0;
    config.vacationMode = parseIntFromFields(fields, "vacation", config.vacationMode ? 1 : 0) != 0;
    config.defaultSleepMinutes = parseIntFromFields(fields, "sleep_minutes", config.defaultSleepMinutes);
    if (const auto it = fields.find("content_mode"); it != fields.end()) {
        config.radioContentMode = it->second == "playlists" ? RadioContentMode::Playlists : RadioContentMode::InternetStations;
    }
    controller.setSystemConfig(config);
}

void addStation(AlarmClockController& controller, const std::map<std::string, std::string>& fields) {
    const auto name = fields.count("name") ? fields.at("name") : "";
    const auto url = fields.count("url") ? fields.at("url") : "";
    if (name.empty() || url.empty()) {
        return;
    }
    controller.stations().push_back(RadioStation { name, url, parseIntFromFields(fields, "frequency", 0) });
}

void addPlaylist(AlarmClockController& controller, const std::map<std::string, std::string>& fields) {
    const auto name = fields.count("name") ? fields.at("name") : "";
    const auto path = fields.count("path") ? fields.at("path") : "";
    if (name.empty() || path.empty()) {
        return;
    }
    controller.playlists().push_back(Playlist { name, path });
}

void addAlarm(AlarmClockController& controller, const std::map<std::string, std::string>& fields) {
    const auto name = fields.count("name") ? fields.at("name") : "";
    if (name.empty()) {
        return;
    }

    int hour = 7;
    int minute = 0;
    if (const auto it = fields.find("time"); it != fields.end()) {
        const auto split = it->second.find(':');
        if (split != std::string::npos) {
            hour = std::stoi(it->second.substr(0, split));
            minute = std::stoi(it->second.substr(split + 1));
        }
    }

    AlarmConfig alarm;
    alarm.name = name;
    alarm.hour = hour;
    alarm.minute = minute;
    alarm.days = alarm_clock::parseDayList(fields.count("days") ? fields.at("days") : "");
    alarm.tone = fields.count("tone") && fields.at("tone") == "buzzer" ? AlarmTone::Buzzer : AlarmTone::Radio;
    alarm.sourceName = fields.count("source") ? fields.at("source") : "";
    alarm.enabled = parseIntFromFields(fields, "enabled", 1) != 0;
    alarm.sleepMinutes = parseIntFromFields(fields, "sleep_minutes", controller.systemConfig().defaultSleepMinutes);
    controller.alarms().push_back(alarm);
}

} // namespace

int main() {
    AlarmClockController controller;
    const auto storageDir = std::filesystem::current_path() / "storage";
    controller.load(storageDir);

    const char* displayIpEnv = std::getenv("ALARM_CLOCK_IP");
    const std::string displayIpAddress = displayIpEnv != nullptr ? displayIpEnv : "192.168.4.1";
    const int port = 8080;

    const int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(server, 8) != 0) {
        std::cerr << "Failed to bind to port " << port << '\n';
        close(server);
        return 1;
    }

    std::cout << "Alarm clock control UI available at http://127.0.0.1:" << port << "/\n";
    std::cout << "Displayed device IP: " << displayIpAddress << '\n';
    std::cout << "Configuration storage: " << storageDir << '\n';

    while (true) {
        const int client = accept(server, nullptr, nullptr);
        if (client < 0) {
            continue;
        }

        const auto now = alarm_clock::Clock::now();
        controller.tick(now);
        const std::string request = readRequest(client);
        if (request.empty()) {
            close(client);
            continue;
        }

        std::istringstream stream(request);
        std::string method;
        std::string path;
        std::string version;
        stream >> method >> path >> version;

        const auto bodyPos = request.find("\r\n\r\n");
        const std::string body = bodyPos == std::string::npos ? std::string() : request.substr(bodyPos + 4);

        if (method == "GET" && path == "/") {
            sendResponse(client, "200 OK", "text/html; charset=utf-8", controller.renderHtml(now, displayIpAddress));
        } else if (method == "GET" && path == "/api/status") {
            sendResponse(client, "200 OK", "application/json; charset=utf-8", controller.statusJson(now, displayIpAddress));
        } else if (method == "POST" && path == "/config/system") {
            applySystemConfig(controller, parseForm(body));
            controller.save(storageDir);
            redirectHome(client);
        } else if (method == "POST" && path == "/config/station") {
            addStation(controller, parseForm(body));
            controller.save(storageDir);
            redirectHome(client);
        } else if (method == "POST" && path == "/config/playlist") {
            addPlaylist(controller, parseForm(body));
            controller.save(storageDir);
            redirectHome(client);
        } else if (method == "POST" && path == "/config/alarm") {
            addAlarm(controller, parseForm(body));
            controller.save(storageDir);
            redirectHome(client);
        } else if (method == "POST" && path == "/action/radio/on") {
            controller.setRadioEnabled(true);
            redirectHome(client);
        } else if (method == "POST" && path == "/action/radio/off") {
            controller.setRadioEnabled(false);
            redirectHome(client);
        } else if (method == "POST" && path == "/action/sleep/start") {
            controller.startSleepMode(now);
            redirectHome(client);
        } else if (method == "POST" && path == "/action/display/ip") {
            controller.setDisplayMode(DisplayMode::IpAddress);
            redirectHome(client);
        } else if (method == "POST" && path == "/action/bluetooth/speaker") {
            controller.pressBluetoothButton(1, now);
            redirectHome(client);
        } else if (method == "POST" && path == "/action/bluetooth/phone") {
            controller.pressBluetoothButton(2, now);
            redirectHome(client);
        } else {
            sendResponse(client, "404 Not Found", "text/plain; charset=utf-8", "Not found");
        }

        close(client);
    }

    close(server);
    return 0;
}
