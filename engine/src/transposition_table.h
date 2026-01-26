#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
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
	struct AtomicEntry {
		std::atomic<std::uint64_t> key{0};
		std::atomic<std::uint64_t> data{0};
	};

	static constexpr std::size_t kEntryCount = 1 << 18;

	std::vector<AtomicEntry> entries_;
	std::size_t mask_ = 0;
};

}
