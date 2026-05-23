#pragma once

#include <Arduino.h>
#include <Audio.h>

#include <SdController.h>

class WebServer;

class SoundController {
 public:
  SoundController(SdController& sdController,
                  uint8_t i2sBclkPin,
                  uint8_t i2sLrclkPin,
                  uint8_t i2sDataPin,
                  uint8_t volume = 5);

  void begin();
  void update();

  void handleListMusicFiles(WebServer& webServer);
  void handleGetStatus(WebServer& webServer);
  void handlePlay(WebServer& webServer);
  void handlePlayRadio(WebServer& webServer);
  void handleStop(WebServer& webServer);
  void handleSetVolume(WebServer& webServer);

  bool isReady() const;
  bool isPlaying() const;
  const String& currentTrack() const;
  uint8_t volume() const;

 private:
  bool ensureAudioReady();
  bool isMusicFilename(const String& name) const;
  String normalizePath(const String& requestedPath) const;
  String normalizeRadioUrl(const String& requestedUrl) const;

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
};
