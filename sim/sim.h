// Shared helpers for the native simulator (desktop only; never compiled into firmware).
#pragma once
#include <string>
#include <vector>

#include "tb_canvas.h"
#include "tb_face.h"

namespace sim {

extern std::vector<std::string> gSets;  // --set overrides
tb::FaceStyle styled(int index);        // style with overrides applied
int styleIndex(char id);                // -1 if unknown

bool writePGM(const std::string& path, const tb::Canvas& c);
bool writeBin(const std::string& path, const tb::Canvas& c);
bool writeAscii(const std::string& path, const tb::Canvas& c);
std::string base64(const uint8_t* data, size_t n);
uint32_t fnv1a(const uint8_t* data, size_t n, uint32_t h = 2166136261u);

int cmdAnim(const std::vector<std::string>& a);   // tbsim anim <id> <face> <outdir> ...
int cmdReel(const std::vector<std::string>& a);   // tbsim reel <face> <seconds> <outdir> ...
int cmdJson(const std::vector<std::string>& a);   // tbsim json anim|reel ... (stdout)
int cmdTest(const std::vector<std::string>& a);   // tbsim test [--json]

// tiny option parser: --key value
std::string opt(const std::vector<std::string>& a, const char* key, const char* def);

}  // namespace sim
