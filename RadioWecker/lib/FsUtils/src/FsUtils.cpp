#include "FsUtils.h"

namespace FsUtils {

bool filterFilename(const String& name, const char* const kSupportedFileExtensions[], size_t kSupportedFileExtensionCount) {
  if (kSupportedFileExtensions == nullptr || kSupportedFileExtensionCount == 0) {
    return true;
  }
  String lower = name;
  lower.toLowerCase();
  for (size_t i = 0; i < kSupportedFileExtensionCount; ++i) {
    if (lower.endsWith(kSupportedFileExtensions[i])) {
      return true;
    }
  }
  return false;
}

JsonDocument collectFiles(fs::FS& fs, const String& dirPath, uint16_t kMaxListedFiles,
                          const char* const kSupportedFileExtensions[], size_t kSupportedFileExtensionCount) {
  JsonDocument doc;
  uint16_t count = 0;

  doc["ok"] = true;
  doc["error"] = "";
  doc["path"] = dirPath;
  doc["truncated"] = false;
  JsonArray folders = doc["folders"].to<JsonArray>();
  JsonArray files = doc["files"].to<JsonArray>();

  File dir = fs.open(dirPath, FILE_READ);
  if (!dir || !dir.isDirectory()) {
    if (dir) {
      dir.close();
    }
    doc["ok"] = false;
    doc["error"] = "Failed to open directory";
    return doc;
  }

  const String prefix = dirPath.endsWith("/") ? dirPath : dirPath + "/";

  File entry = dir.openNextFile();
  while (entry) {
    String name = String(entry.name());
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) {
      name = name.substring(slash + 1);
    }
    const bool isDir = entry.isDirectory();
    entry.close();

    if (name.length() > 0 && !name.startsWith(".")) {
      if (count >= kMaxListedFiles) {
        doc["truncated"] = true;
        break;
      }

      const String fullPath = prefix + name;
      if (isDir) {
        JsonObject folderObj = folders.add<JsonObject>();
        folderObj["name"] = name;
        folderObj["path"] = fullPath;
        count++;
      } else if (filterFilename(name, kSupportedFileExtensions, kSupportedFileExtensionCount)) {
        JsonObject fileObj = files.add<JsonObject>();
        fileObj["name"] = name;
        fileObj["path"] = fullPath;
        count++;
      }
    }

    entry = dir.openNextFile();
  }

  dir.close();
  return doc;
}

}  // namespace FsUtils
