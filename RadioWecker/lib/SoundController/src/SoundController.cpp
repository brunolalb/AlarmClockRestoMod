#include "SoundController.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <WebServer.h>

namespace {
void warmUpAudioDecoder(Audio& audio) {
  for (uint8_t i = 0; i < 24; ++i) {
    audio.loop();
    delay(1);
  }
}
}

SoundController::SoundController(SdController& sdController,
                                 uint8_t i2sBclkPin,
                                 uint8_t i2sLrclkPin,
                                 uint8_t i2sDataPin,
                                 uint8_t volume)
    : sdController_(sdController),
      audio_(),
      i2sBclkPin_(i2sBclkPin),
      i2sLrclkPin_(i2sLrclkPin),
      i2sDataPin_(i2sDataPin),
      volume_(volume > 21 ? 21 : volume) {}

void SoundController::begin() {
  ensureAudioReady();
  //audio_.connecttohost("0n-80s.radionetz.de:8000/0n-70s.mp3");
}

void SoundController::update() {
  if (!ensureAudioReady()) {
    return;
  }

  audio_.loop();
  playing_ = audio_.isRunning();

  if (!playing_) {
    paused_ = false;
  }
}

bool SoundController::isReady() const {
  return audioReady_;
}

bool SoundController::isPlaying() const {
  return audioReady_ && playing_;
}

const String& SoundController::currentTrack() const {
  return currentTrack_;
}

uint8_t SoundController::volume() const {
  return volume_;
}

bool SoundController::ensureAudioReady() {
  if (audioReady_) {
    return true;
  }

  if (!sdController_.isReady()) {
    return false;
  }

  audioReady_ = audio_.setPinout(i2sBclkPin_, i2sLrclkPin_, i2sDataPin_);
  if (!audioReady_) {
    return false;
  }

  audio_.setVolume(volume_);
  return true;
}

bool SoundController::isMusicFilename(const String& name) const {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".mp3") || lower.endsWith(".wav") || lower.endsWith(".ogg");
}

String SoundController::normalizePath(const String& requestedPath) const {
  String path = requestedPath;
  path.trim();
  if (path.length() == 0) {
    return String();
  }

  if (path.indexOf("..") >= 0) {
    return String();
  }

  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  return path;
}

String SoundController::normalizeRadioUrl(const String& requestedUrl) const {
  String url = requestedUrl;
  url.trim();
  if (url.length() == 0) {
    return String();
  }

  if (!(url.startsWith("http://") || url.startsWith("https://"))) {
    return String();
  }

  return url;
}

void SoundController::handleListMusicFiles(WebServer& webServer) {
  DynamicJsonDocument doc(4096);
  JsonArray files = doc.to<JsonArray>();

  if (sdController_.isReady()) {
    File root = SD.open("/");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          String name = String(file.name());
          if (isMusicFilename(name)) {
            files.add(name);
          }
        }
        file.close();
        file = root.openNextFile();
      }
      root.close();
    }
  }

  String payload;
  serializeJson(files, payload);
  webServer.send(200, "application/json", payload);
}

void SoundController::handleGetStatus(WebServer& webServer) {
  StaticJsonDocument<512> doc;
  doc["ready"] = isReady();
  doc["playing"] = isPlaying();
  doc["paused"] = paused_;
  doc["track"] = currentTrack_;
  doc["volume"] = volume_;

  if (isReady()) {
    doc["positionSec"] = audio_.getAudioCurrentTime();
    doc["durationSec"] = audio_.getAudioFileDuration();
  } else {
    doc["positionSec"] = 0;
    doc["durationSec"] = 0;
  }

  String payload;
  serializeJson(doc, payload);
  webServer.send(200, "application/json", payload);
}

void SoundController::handlePlay(WebServer& webServer) {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing JSON body\"}");
    return;
  }

  StaticJsonDocument<256> doc;
  const DeserializationError err = deserializeJson(doc, webServer.arg("plain"));
  if (err) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  const String requestedPath = doc["path"] | "";
  const String path = normalizePath(requestedPath);
  if (path.length() == 0 || !isMusicFilename(path)) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid music path\"}");
    return;
  }

  String playbackPath = path;
  if (!SD.exists(playbackPath) && playbackPath.startsWith("/")) {
    const String noSlashPath = playbackPath.substring(1);
    if (SD.exists(noSlashPath)) {
      playbackPath = noSlashPath;
    }
  }

  if (!sdController_.isReady()) {
    webServer.send(503, "application/json", "{\"ok\":false,\"error\":\"SD not ready\"}");
    return;
  }

  if (!SD.exists(playbackPath)) {
    webServer.send(404, "application/json", "{\"ok\":false,\"error\":\"File not found\"}");
    return;
  }

  File mediaFile = SD.open(playbackPath, FILE_READ);
  const size_t mediaSize = mediaFile ? static_cast<size_t>(mediaFile.size()) : 0;
  if (mediaFile) {
    mediaFile.close();
  }

  if (!ensureAudioReady()) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"Audio init failed\"}");
    return;
  }

  audio_.stopSong();
  if (!audio_.connecttoFS(SD, playbackPath.c_str())) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"Playback start failed\"}");
    return;
  }

  warmUpAudioDecoder(audio_);

  currentTrack_ = playbackPath;
  playing_ = true;
  paused_ = false;

  StaticJsonDocument<192> response;
  response["ok"] = true;
  response["track"] = currentTrack_;
  String payload;
  serializeJson(response, payload);
  webServer.send(200, "application/json", payload);
}

void SoundController::handlePlayRadio(WebServer& webServer) {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing JSON body\"}");
    return;
  }

  StaticJsonDocument<256> doc;
  const DeserializationError err = deserializeJson(doc, webServer.arg("plain"));
  if (err) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  const String requestedUrl = doc["url"] | "";
  const String url = normalizeRadioUrl(requestedUrl);
  if (url.length() == 0) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid radio URL\"}");
    return;
  }

  if (!ensureAudioReady()) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"Audio init failed\"}");
    return;
  }

  audio_.stopSong();
  if (!audio_.connecttohost(url.c_str())) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"Radio stream start failed\"}");
    return;
  }

  warmUpAudioDecoder(audio_);

  currentTrack_ = url;
  playing_ = true;
  paused_ = false;

  StaticJsonDocument<192> response;
  response["ok"] = true;
  response["track"] = currentTrack_;
  String payload;
  serializeJson(response, payload);
  webServer.send(200, "application/json", payload);
}

void SoundController::handleStop(WebServer& webServer) {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  if (audioReady_) {
    audio_.stopSong();
  }
  playing_ = false;
  paused_ = false;

  StaticJsonDocument<128> response;
  response["ok"] = true;
  String payload;
  serializeJson(response, payload);
  webServer.send(200, "application/json", payload);
}

void SoundController::handleSetVolume(WebServer& webServer) {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing JSON body\"}");
    return;
  }

  StaticJsonDocument<128> doc;
  const DeserializationError err = deserializeJson(doc, webServer.arg("plain"));
  if (err || !doc.containsKey("volume")) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  int nextVolume = doc["volume"] | static_cast<int>(volume_);
  if (nextVolume < 0) {
    nextVolume = 0;
  }
  if (nextVolume > 21) {
    nextVolume = 21;
  }

  volume_ = static_cast<uint8_t>(nextVolume);
  if (ensureAudioReady()) {
    audio_.setVolume(volume_);
  }

  StaticJsonDocument<128> response;
  response["ok"] = true;
  response["volume"] = volume_;
  String payload;
  serializeJson(response, payload);
  webServer.send(200, "application/json", payload);
}
