#pragma once
#include "tb_anim.h"

namespace tb {

// Animation catalog. Milestone 1 ships one fully choreographed animation (#80) to validate
// the engine and the visual direction; the remaining 79 are added after design approval.
int animCount();
const AnimDef& animAt(int index);
const AnimDef* animById(int id);
const AnimDef* animByName(const char* name);  // case-insensitive; accepts unique prefixes/substrings

}  // namespace tb
