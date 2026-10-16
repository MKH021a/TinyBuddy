// Shared runtime state of the firmware (one instance, statically allocated).
#pragma once
#include <Arduino.h>

#include "Config.h"
#include "DisplayAdapter.h"
#include "tb_buddy.h"

// Rolling frame statistics (1 s windows) — the numbers behind the `fps` command.
struct FrameStats {
  uint32_t total = 0;
  float fps = 0.f;
  uint32_t renderAvgUs = 0, renderMaxUs = 0, presentAvgUs = 0, presentMaxUs = 0;
  uint32_t minFreeHeap = UINT32_MAX;
  void add(uint32_t nowMs, uint32_t renderUs, uint32_t presentUs);

 private:
  uint32_t winStart_ = 0, winFrames_ = 0;
  uint64_t rSum_ = 0, pSum_ = 0;
  uint32_t rMax_ = 0, pMax_ = 0;
};

// `test all`: plays every registered animation (both mirror variants) back to back,
// deterministically, and checks timing and effect cleanup.
struct TestRunner {
  bool active = false;
  void start(uint32_t now);
  void update(uint32_t now);

 private:
  void next(uint32_t now);
  int index_ = 0, mirror_ = 0, failures_ = 0, runs_ = 0;
  uint32_t started_ = 0, expected_ = 0, worstRenderUs_ = 0;
  bool wasAuto_ = true;
};

struct App {
  tb::Buddy buddy;
  tb::FaceStyle style;   // working copy of the selected candidate (`set` tweaks apply here)
  char styleId = cfg::kDefaultFace;
  DisplayAdapter display;
  FrameStats stats;
  TestRunner test;
  bool pattern = false;  // controller test pattern instead of the face
  uint32_t lastFrameRenderUs = 0;

  void selectStyle(char id);
  void savePrefs();
};

extern App app;
