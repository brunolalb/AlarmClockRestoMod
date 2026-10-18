#pragma once

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

namespace FsUtils {

bool filterFilename(const String& name, const char* const kSupportedFileExtensions[], size_t kSupportedFileExtensionCount);

// Lists a single directory (non-recursive).
// Result: { ok, error, path, folders: [{name, path}], files: [{name, path}], truncated }
JsonDocument collectFiles(fs::FS& fs, const String& dirPath, uint16_t kMaxListedFiles,
                          const char* const kSupportedFileExtensions[], size_t kSupportedFileExtensionCount);

}  // namespace FsUtils
