#include "SdController.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <WebServer.h>

namespace {
String normalizeFsPath(const String& rawPath, bool ensureTrailingSlash) {
  String path = rawPath;
  path.trim();
  path.replace("\\", "/");
  while (path.indexOf("//") >= 0) {
    path.replace("//", "/");
  }

  if (path.length() == 0) {
    path = "/";
  }

  if (path.indexOf("..") >= 0) {
    return String();
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

bool hasAllowedExtension(const String& name, const char* const* allowedExtensions, size_t allowedExtensionCount) {
  String lower = name;
  lower.toLowerCase();

  for (size_t i = 0; i < allowedExtensionCount; ++i) {
    if (lower.endsWith(allowedExtensions[i])) {
      return true;
    }
  }

  return false;
}
}

SdController::SdController(uint8_t csPin,
                           uint8_t spiSckPin,
                           uint8_t spiMisoPin,
                           uint8_t spiMosiPin,
                           uint32_t spiFrequencyHz,
                           SPIClass& spiBus)
    : csPin_(csPin),
      spiSckPin_(spiSckPin),
      spiMisoPin_(spiMisoPin),
      spiMosiPin_(spiMosiPin),
      spiFrequencyHz_(spiFrequencyHz),
      spiBus_(&spiBus) {}

bool SdController::runSelfTest() {
  static const char* kTestPath = "/sdtest.txt";
  static const char* kMarker = "TM1637_SD_OK";

  if (SD.exists(kTestPath) && !SD.remove(kTestPath)) {
    return false;
  }

  File out = SD.open(kTestPath, FILE_WRITE);
  if (!out) {
    return false;
  }
  out.println(kMarker);
  out.close();

  File in = SD.open(kTestPath, FILE_READ);
  if (!in) {
    return false;
  }

  String line = in.readStringUntil('\n');
  in.close();
  line.trim();

  return line == kMarker;
}

SdController::InitResult SdController::initialize() {
  spiBus_->begin(spiSckPin_, spiMisoPin_, spiMosiPin_, csPin_);
  ready_ = SD.begin(csPin_, *spiBus_, spiFrequencyHz_);
  selfTestPassed_ = false;

  if (!ready_) {
    return {ready_, selfTestPassed_};
  }

  selfTestPassed_ = runSelfTest();
  return {ready_, selfTestPassed_};
}

bool SdController::isReady() const {
  return ready_;
}

bool SdController::selfTestPassed() const {
  return selfTestPassed_;
}

uint64_t SdController::totalBytes() const {
  if (!ready_) {
    return 0;
  }

  return SD.totalBytes();
}

uint64_t SdController::usedBytes() const {
  if (!ready_) {
    return 0;
  }

  return SD.usedBytes();
}

uint64_t SdController::availableBytes() const {
  const uint64_t total = totalBytes();
  const uint64_t used = usedBytes();
  return used <= total ? (total - used) : 0;
}

bool SdController::exists(const String& path) const {
  if (!ready_) {
    return false;
  }

  return SD.exists(path);
}

bool SdController::remove(const String& path) {
  if (!ready_) {
    return false;
  }

  return SD.remove(path);
}

bool SdController::mkdir(const String& path) {
  if (!ready_) {
    return false;
  }

  return SD.mkdir(path);
}

bool SdController::rmdir(const String& path) {
  if (!ready_) {
    return false;
  }

  return SD.rmdir(path);
}

File SdController::open(const String& path, const char* mode) {
  if (!ready_) {
    return File();
  }

  return SD.open(path, mode);
}

fs::FS& SdController::fs() {
  return SD;
}

void SdController::handleListFiles(WebServer& webServer,
                                   const char* const* allowedExtensions,
                                   size_t allowedExtensionCount) {
  String requestPath = "/";
  if (webServer.hasArg("path")) {
    requestPath = normalizeFsPath(webServer.arg("path"), false);
    if (requestPath.length() == 0) {
      webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid path\"}");
      return;
    }
  }

  StaticJsonDocument<4096> doc;
  JsonObject root = doc.to<JsonObject>();
  JsonArray folders = root.createNestedArray("folders");
  JsonArray files = root.createNestedArray("files");

  if (!ready_) {
    String payload;
    serializeJson(doc, payload);
    webServer.send(200, "application/json", payload);
    return;
  }

  const String normalizedPath = normalizeFsPath(requestPath, true);
  String openPath = normalizedPath;
  if (openPath.length() > 1 && openPath.endsWith("/")) {
    openPath.remove(openPath.length() - 1);
  }

  File dir = open(openPath, FILE_READ);
  if (!dir || !dir.isDirectory()) {
    dir = open(normalizedPath, FILE_READ);
  }
  if (!dir || !dir.isDirectory()) {
    webServer.send(404, "application/json", "{\"ok\":false,\"error\":\"Path not found\"}");
    return;
  }

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
    } else if (hasAllowedExtension(name, allowedExtensions, allowedExtensionCount)) {
      JsonObject fileObj = files.createNestedObject();
      fileObj["name"] = name;
      fileObj["path"] = joinDirAndName(normalizedPath, name, false);
    }

    file.close();
    file = dir.openNextFile();
  }

  dir.close();

  String payload;
  serializeJson(doc, payload);
  webServer.send(200, "application/json", payload);
}
