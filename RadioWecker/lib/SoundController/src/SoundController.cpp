#include "SoundController.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>

namespace {
void warmUpAudioDecoder(Audio& audio) {
  for (uint8_t i = 0; i < 24; ++i) {
    audio.loop();
    delay(1);
  }
}

String trimTagText(const char* buffer, size_t len) {
  if (buffer == nullptr || len == 0) {
    return String();
  }

  size_t start = 0;
  while (start < len && (buffer[start] == ' ' || buffer[start] == '\0')) {
    ++start;
  }

  if (start >= len) {
    return String();
  }

  size_t end = len;
  while (end > start) {
    const char c = buffer[end - 1];
    if (c == ' ' || c == '\0') {
      --end;
      continue;
    }
    break;
  }

  if (end <= start) {
    return String();
  }

  String out;
  out.reserve(end - start);
  for (size_t i = start; i < end; ++i) {
    out += buffer[i];
  }
  return out;
}

String upperExtensionFromPath(const String& path) {
  const int dotIdx = path.lastIndexOf('.');
  if (dotIdx < 0 || dotIdx + 1 >= static_cast<int>(path.length())) {
    return String("Unknown");
  }

  String ext = path.substring(dotIdx + 1);
  ext.toUpperCase();
  return ext;
}

String folderFromPath(const String& path) {
  const int slashIdx = path.lastIndexOf('/');
  if (slashIdx <= 0) {
    return String("/");
  }
  return path.substring(0, slashIdx);
}

String titleFromPath(const String& path) {
  int slashIdx = path.lastIndexOf('/');
  if (slashIdx < 0) {
    slashIdx = -1;
  }

  const int dotIdx = path.lastIndexOf('.');
  const int endIdx = dotIdx > slashIdx ? dotIdx : static_cast<int>(path.length());
  if (endIdx <= slashIdx + 1) {
    return String();
  }
  return path.substring(slashIdx + 1, endIdx);
}

String normalizeFsPath(const String& rawPath, bool ensureTrailingSlash) {
  String path = rawPath;
  path.replace("\\", "/");
  while (path.indexOf("//") >= 0) {
    path.replace("//", "/");
  }

  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  if (ensureTrailingSlash) {
    if (!path.endsWith("/")) {
      path += "/";
    }
  } else if (path.length() > 1 && path.endsWith("/")) {
    path.remove(path.length() - 1);
  }

  return path;
}

String entryBaseName(const String& rawName) {
  String name = rawName;
  name.replace("\\", "/");
  while (name.endsWith("/")) {
    name.remove(name.length() - 1);
  }

  const int slash = name.lastIndexOf('/');
  if (slash >= 0) {
    name = name.substring(slash + 1);
  }

  return name;
}

String joinDirAndName(const String& dirPath, const String& name, bool asDirectory) {
  String fullPath = normalizeFsPath(dirPath, true) + name;
  return normalizeFsPath(fullPath, asDirectory);
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

  if (playing_) {
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

bool SoundController::resolveLocalPlaybackPath(const String& path, String& playbackPath) const {
  if (!sdController_.isReady()) {
    return false;
  }

  playbackPath = path;
  if (sdController_.exists(playbackPath)) {
    return true;
  }

  if (playbackPath.startsWith("/")) {
    const String noSlashPath = playbackPath.substring(1);
    if (sdController_.exists(noSlashPath)) {
      playbackPath = noSlashPath;
      return true;
    }
  }

  return false;
}

bool SoundController::startLocalTrack(const String& playbackPath, String& error) {
  if (!ensureAudioReady()) {
    error = "Audio init failed";
    return false;
  }

  audio_.stopSong();
  if (!audio_.connecttoFS(sdController_.fs(), playbackPath.c_str())) {
    error = "Playback start failed";
    return false;
  }

  warmUpAudioDecoder(audio_);

  currentTrack_ = playbackPath;
  populateTrackMetadataFromFile(playbackPath);
  playing_ = true;
  paused_ = false;
  return true;
}

void SoundController::clearTrackMetadata() {
  trackTitle_ = "";
  trackArtist_ = "";
  trackAlbum_ = "";
  trackYear_ = "";
  trackFormat_ = "";
  trackFolder_ = "";
}

void SoundController::populateTrackMetadataFromFile(const String& playbackPath) {
  clearTrackMetadata();

  String normalizedPath = playbackPath;
  if (!normalizedPath.startsWith("/")) {
    normalizedPath = "/" + normalizedPath;
  }

  trackTitle_ = titleFromPath(normalizedPath);
  trackFormat_ = upperExtensionFromPath(normalizedPath);
  trackFolder_ = folderFromPath(normalizedPath);

  File trackFile = sdController_.open(normalizedPath, FILE_READ);
  if (!trackFile) {
    trackFile = sdController_.open(playbackPath, FILE_READ);
  }

  if (!trackFile) {
    return;
  }

  const size_t size = static_cast<size_t>(trackFile.size());
  if (size >= 128) {
    if (trackFile.seek(size - 128)) {
      char tag[128];
      const size_t readCount = trackFile.read(reinterpret_cast<uint8_t*>(tag), sizeof(tag));
      if (readCount == sizeof(tag) && tag[0] == 'T' && tag[1] == 'A' && tag[2] == 'G') {
        const String id3Title = trimTagText(&tag[3], 30);
        const String id3Artist = trimTagText(&tag[33], 30);
        const String id3Album = trimTagText(&tag[63], 30);
        const String id3Year = trimTagText(&tag[93], 4);

        if (id3Title.length() > 0) {
          trackTitle_ = id3Title;
        }
        trackArtist_ = id3Artist;
        trackAlbum_ = id3Album;
        trackYear_ = id3Year;
      }
    }
  }

  trackFile.close();
}

bool SoundController::listMusicFiles(JsonArray& files) const {
  if (!sdController_.isReady()) {
    return false;
  }

  File root = sdController_.open("/", FILE_READ);
  if (!root || !root.isDirectory()) {
    return false;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      const String name = String(file.name());
      if (isMusicFilename(name)) {
        files.add(name);
      }
    }
    file.close();
    file = root.openNextFile();
  }

  root.close();
  return true;
}

bool SoundController::findNextMusicFile(String& nextTrack) const {
  nextTrack = String();
  if (!sdController_.isReady()) {
    return false;
  }

  const String currentNoSlash = currentTrack_.startsWith("/") ? currentTrack_.substring(1) : currentTrack_;
  const String currentWithSlash = currentTrack_.startsWith("/") ? currentTrack_ : ("/" + currentTrack_);

  File root = sdController_.open("/", FILE_READ);
  if (!root || !root.isDirectory()) {
    return false;
  }

  String firstTrack;
  bool foundCurrent = false;

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      const String name = String(file.name());
      if (isMusicFilename(name)) {
        if (firstTrack.length() == 0) {
          firstTrack = name;
        }

        if (foundCurrent) {
          nextTrack = name;
          file.close();
          root.close();
          return true;
        }

        if (name == currentTrack_ || name == currentNoSlash || name == currentWithSlash) {
          foundCurrent = true;
        }
      }
    }
    file.close();
    file = root.openNextFile();
  }

  root.close();

  if (firstTrack.length() > 0) {
    nextTrack = firstTrack;
    return true;
  }

  return false;
}

bool SoundController::findPreviousMusicFile(String& prevTrack) const {
  prevTrack = String();
  if (!sdController_.isReady()) {
    return false;
  }

  const String currentNoSlash = currentTrack_.startsWith("/") ? currentTrack_.substring(1) : currentTrack_;
  const String currentWithSlash = currentTrack_.startsWith("/") ? currentTrack_ : ("/" + currentTrack_);

  File root = sdController_.open("/", FILE_READ);
  if (!root || !root.isDirectory()) {
    return false;
  }

  String lastTrack;
  bool foundCurrent = false;

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      const String name = String(file.name());
      if (isMusicFilename(name)) {
        if (name == currentTrack_ || name == currentNoSlash || name == currentWithSlash) {
          foundCurrent = true;
          if (lastTrack.length() > 0) {
            prevTrack = lastTrack;
            file.close();
            root.close();
            return true;
          }
          break;
        }
        lastTrack = name;
      }
    }
    file.close();
    file = root.openNextFile();
  }

  root.close();

  if (foundCurrent && lastTrack.length() > 0) {
    prevTrack = lastTrack;
    return true;
  }

  if (lastTrack.length() > 0) {
    prevTrack = lastTrack;
    return true;
  }

  return false;
}

void SoundController::handleListMusicFiles(WebServer& webServer) {
  // Get requested directory path from query string, default to root
  String requestPath = "/";
  if (webServer.hasArg("path")) {
    requestPath = normalizePath(webServer.arg("path"));
    if (requestPath.length() == 0) {
      requestPath = "/";
    }
  }

  StaticJsonDocument<4096> doc;
  JsonObject root = doc.to<JsonObject>();

  if (sdController_.isReady()) {
    buildFileTree(requestPath, root);
  }

  String payload;
  serializeJson(doc, payload);
  webServer.send(200, "application/json", payload);
}

void SoundController::buildFileTree(const String& path, JsonObject& parentObj) const {
  if (!sdController_.isReady()) {
    return;
  }

  const String normalizedPath = normalizeFsPath(path, true);
  String openPath = normalizedPath;
  if (openPath.length() > 1 && openPath.endsWith("/")) {
    openPath.remove(openPath.length() - 1);
  }

  File dir = sdController_.open(openPath, FILE_READ);
  if (!dir || !dir.isDirectory()) {
    dir = sdController_.open(normalizedPath, FILE_READ);
  }
  if (!dir || !dir.isDirectory()) {
    return;
  }

  // Only list immediate children (single level, non-recursive)
  JsonArray folders = parentObj.createNestedArray("folders");
  JsonArray files = parentObj.createNestedArray("files");

  File file = dir.openNextFile();
  while (file) {
    const String rawName = String(file.name());
    const String name = entryBaseName(rawName);

    if (name.length() == 0) {
      file.close();
      file = dir.openNextFile();
      continue;
    }
    
    if (file.isDirectory()) {
      JsonObject folderObj = folders.createNestedObject();
      folderObj["name"] = name;
      folderObj["path"] = joinDirAndName(normalizedPath, name, true);
    } else if (isMusicFilename(name)) {
      JsonObject fileObj = files.createNestedObject();
      fileObj["name"] = name;
      fileObj["path"] = joinDirAndName(normalizedPath, name, false);
    }
    
    file.close();
    file = dir.openNextFile();
  }

  dir.close();
}

void SoundController::handleGetStatus(WebServer& webServer) {
  StaticJsonDocument<768> doc;
  doc["ready"] = isReady();
  doc["playing"] = isPlaying();
  doc["paused"] = paused_;
  doc["track"] = currentTrack_;
  doc["trackTitle"] = trackTitle_;
  doc["trackArtist"] = trackArtist_;
  doc["trackAlbum"] = trackAlbum_;
  doc["trackYear"] = trackYear_;
  doc["trackFormat"] = trackFormat_;
  doc["trackFolder"] = trackFolder_;
  doc["volume"] = volume_;

  if (isReady()) {
    const uint32_t positionSec = audio_.getAudioCurrentTime();
    const uint32_t durationSec = audio_.getAudioFileDuration();
    doc["positionSec"] = positionSec;
    doc["durationSec"] = durationSec;
    doc["progressPct"] = durationSec > 0 ? (100.0 * static_cast<double>(positionSec) / static_cast<double>(durationSec)) : 0.0;
  } else {
    doc["positionSec"] = 0;
    doc["durationSec"] = 0;
    doc["progressPct"] = 0.0;
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

  if (!sdController_.isReady()) {
    webServer.send(503, "application/json", "{\"ok\":false,\"error\":\"SD not ready\"}");
    return;
  }

  String playbackPath;
  if (!resolveLocalPlaybackPath(path, playbackPath)) {
    webServer.send(404, "application/json", "{\"ok\":false,\"error\":\"File not found\"}");
    return;
  }

  String error;
  if (!startLocalTrack(playbackPath, error)) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"" + error + "\"}");
    return;
  }

  StaticJsonDocument<192> response;
  response["ok"] = true;
  response["track"] = currentTrack_;
  String payload;
  serializeJson(response, payload);
  webServer.send(200, "application/json", payload);
}

void SoundController::handlePauseToggle(WebServer& webServer) {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  if (!ensureAudioReady()) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"Audio init failed\"}");
    return;
  }

  if (currentTrack_.length() == 0 || (!playing_ && !paused_)) {
    webServer.send(409, "application/json", "{\"ok\":false,\"error\":\"Nothing to pause or resume\"}");
    return;
  }

  if (!audio_.pauseResume()) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"Pause/resume failed\"}");
    return;
  }

  paused_ = !paused_;
  playing_ = !paused_;

  StaticJsonDocument<160> response;
  response["ok"] = true;
  response["paused"] = paused_;
  response["playing"] = playing_;
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

  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(503, "application/json", "{\"ok\":false,\"error\":\"WiFi not connected\"}");
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
  clearTrackMetadata();
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
  currentTrack_ = "";
  clearTrackMetadata();

  StaticJsonDocument<128> response;
  response["ok"] = true;
  String payload;
  serializeJson(response, payload);
  webServer.send(200, "application/json", payload);
}

void SoundController::handleNext(WebServer& webServer) {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  String nextTrack;
  if (!findNextMusicFile(nextTrack)) {
    webServer.send(404, "application/json", "{\"ok\":false,\"error\":\"No music files found\"}");
    return;
  }

  String playbackPath;
  if (!resolveLocalPlaybackPath(nextTrack, playbackPath)) {
    webServer.send(404, "application/json", "{\"ok\":false,\"error\":\"Next track not found\"}");
    return;
  }

  String error;
  if (!startLocalTrack(playbackPath, error)) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"" + error + "\"}");
    return;
  }

  StaticJsonDocument<192> response;
  response["ok"] = true;
  response["track"] = currentTrack_;
  String payload;
  serializeJson(response, payload);
  webServer.send(200, "application/json", payload);
}

void SoundController::handlePrevious(WebServer& webServer) {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  String prevTrack;
  if (!findPreviousMusicFile(prevTrack)) {
    webServer.send(404, "application/json", "{\"ok\":false,\"error\":\"No music files found\"}");
    return;
  }

  String playbackPath;
  if (!resolveLocalPlaybackPath(prevTrack, playbackPath)) {
    webServer.send(404, "application/json", "{\"ok\":false,\"error\":\"Previous track not found\"}");
    return;
  }

  String error;
  if (!startLocalTrack(playbackPath, error)) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"" + error + "\"}");
    return;
  }

  StaticJsonDocument<192> response;
  response["ok"] = true;
  response["track"] = currentTrack_;
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
