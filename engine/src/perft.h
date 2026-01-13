#pragma once

#include <cstdint>

#include "position.h"

namespace flare {

std::uint64_t Perft(Position& position, int depth);

}

