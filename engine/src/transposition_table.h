#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "move.h"

namespace flare {

enum class Bound : std::uint8_t {
	kExact = 0,
	kLower = 1,
	kUpper = 2,
};

struct TranspositionEntry {
	std::uint64_t key = 0;
	Move best_move = kNoMove;
	int depth = -1;
	int score = 0;
	Bound bound = Bound::kExact;
};

class TranspositionTable {
public:
	TranspositionTable();

	void Clear();
	bool Probe(std::uint64_t key, TranspositionEntry& entry) const;
	void Store(std::uint64_t key, int depth, int score, Bound bound, Move best_move);

private:
	static constexpr std::size_t kEntryCount = 1 << 18;
	static constexpr std::size_t kLockCount = 1 << 12;

	std::vector<TranspositionEntry> entries_;
	mutable std::vector<std::mutex> locks_;
	std::size_t mask_ = 0;
	std::size_t lock_mask_ = 0;
};

}

