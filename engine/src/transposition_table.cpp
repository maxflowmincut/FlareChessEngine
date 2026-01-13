#include "transposition_table.h"

namespace flare {

TranspositionTable::TranspositionTable()
	: entries_(kEntryCount),
	  locks_(kLockCount),
	  mask_(kEntryCount - 1),
	  lock_mask_(kLockCount - 1) {}

void TranspositionTable::Clear() {
	entries_.assign(entries_.size(), TranspositionEntry{});
}

bool TranspositionTable::Probe(std::uint64_t key, TranspositionEntry& entry) const {
	std::lock_guard<std::mutex> lock(locks_[key & lock_mask_]);
	const auto& stored = entries_[key & mask_];
	if (stored.key == key) {
		entry = stored;
		return true;
	}
	return false;
}

void TranspositionTable::Store(std::uint64_t key, int depth, int score, Bound bound,
	Move best_move) {
	std::lock_guard<std::mutex> lock(locks_[key & lock_mask_]);
	auto& stored = entries_[key & mask_];
	if (stored.key != key || depth >= stored.depth) {
		stored.key = key;
		stored.depth = depth;
		stored.score = score;
		stored.bound = bound;
		stored.best_move = best_move;
	}
}

}
