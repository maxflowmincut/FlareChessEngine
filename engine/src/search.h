#pragma once

#include <cstdint>

#include "move.h"
#include "position.h"
#include "transposition_table.h"

namespace flare {

struct SearchResult {
	Move best_move = kNoMove;
	int score = 0;
	int depth = 0;
	std::uint64_t nodes = 0;
};

SearchResult Search(Position& position, int max_depth, TranspositionTable& table, int threads = 1);

}

