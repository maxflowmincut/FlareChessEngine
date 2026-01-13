#pragma once

#include <string>
#include <string_view>

#include "position.h"

namespace flare {

bool LoadFen(Position& position, std::string_view fen);
std::string ToFen(const Position& position);

}

