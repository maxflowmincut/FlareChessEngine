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

struct SearchLimits {
	int max_depth = 0;
	std::int64_t time_ms = 0;
};

SearchResult Search(Position& position, int max_depth, TranspositionTable& table, int threads = 1);
SearchResult Search(Position& position, const SearchLimits& limits, TranspositionTable& table,
	int threads = 1);

}
