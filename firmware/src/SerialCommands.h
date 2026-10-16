#pragma once
#include <stdint.h>

// Line-based, case-insensitive command console on Serial (115200 baud).
// Non-blocking: poll() consumes whatever bytes are available and returns immediately.
class SerialCommands {
 public:
  void poll(uint32_t now);
  static void printHelp();

 private:
  void exec(char* line, uint32_t now);
  char line_[96];
  uint8_t len_ = 0;
  bool overflow_ = false;
};
