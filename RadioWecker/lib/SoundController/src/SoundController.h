#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Audio.h>

#include <SdController.h>

class WebServer;

class SoundController {
 public:
  enum class WebServerCommand {
    GetStatus,
    Play,
    PlayRadio,
    PauseToggle,
    Stop,
    Next,
    Previous,
    SetVolume,
  };

  SoundController(SdController& sdController,
                  uint8_t i2sBclkPin,
                  uint8_t i2sLrclkPin,
                  uint8_t i2sDataPin,
                  uint8_t volume = 5);

  static const char* const kSupportedFileExtensions[];
  static constexpr size_t kSupportedFileExtensionCount = 3;

  void begin();
  void update();

  void handleWebServerCommand(WebServer& webServer, WebServerCommand command);

  bool isReady() const;
  bool isPlaying() const;
  const String& currentTrack() const;
  uint8_t volume() const;

 private:
  bool ensureAudioReady();
  bool isMusicFilename(const String& name) const;
  String normalizePath(const String& requestedPath) const;
  String normalizeRadioUrl(const String& requestedUrl) const;
  bool resolveLocalPlaybackPath(const String& path, String& playbackPath) const;
  bool startLocalTrack(const String& playbackPath, String& error);
  bool findNextMusicFile(String& nextTrack) const;
  bool findPreviousMusicFile(String& prevTrack) const;
  void clearTrackMetadata();
  void populateTrackMetadataFromFile(const String& playbackPath);

  SdController& sdController_;
  Audio audio_;
  uint8_t i2sBclkPin_;
  uint8_t i2sLrclkPin_;
  uint8_t i2sDataPin_;
  uint8_t volume_;
  bool audioReady_ = false;
  bool playing_ = false;
  bool paused_ = false;
  String currentTrack_;
  String trackTitle_;
  String trackArtist_;
  String trackAlbum_;
  String trackYear_;
  String trackFormat_;
  String trackFolder_;
};
