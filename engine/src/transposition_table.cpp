#include "transposition_table.h"

namespace flare {
namespace {

constexpr std::uint64_t kMoveMask = 0xFFFFFFFFULL;
constexpr std::uint64_t kScoreMask = 0xFFFFULL;
constexpr std::uint64_t kDepthMask = 0xFFULL;
constexpr std::uint64_t kBoundMask = 0x3ULL;

constexpr int kDepthBias = 1;
constexpr int kMaxDepthStored = 254;

int ClampScore(int score) {
	if (score > 32767) {
		return 32767;
	}
	if (score < -32768) {
		return -32768;
	}
	return score;
}

int ClampDepth(int depth) {
	if (depth < 0) {
		return 0;
	}
	if (depth > kMaxDepthStored) {
		return kMaxDepthStored;
	}
	return depth;
}

std::uint64_t PackEntry(Move best_move, int score, int depth, Bound bound) {
	std::uint32_t move_bits = static_cast<std::uint32_t>(best_move);
	int clamped_score = ClampScore(score);
	std::uint16_t score_bits = static_cast<std::uint16_t>(static_cast<std::int16_t>(clamped_score));
	int clamped_depth = ClampDepth(depth);
	std::uint8_t depth_bits = static_cast<std::uint8_t>(clamped_depth + kDepthBias);
	std::uint8_t bound_bits = static_cast<std::uint8_t>(bound) & 0x3;

	std::uint64_t packed = 0;
	packed |= static_cast<std::uint64_t>(move_bits);
	packed |= static_cast<std::uint64_t>(score_bits) << 32;
	packed |= static_cast<std::uint64_t>(depth_bits) << 48;
	packed |= static_cast<std::uint64_t>(bound_bits) << 56;
	return packed;
}

Move UnpackMove(std::uint64_t packed) {
	return static_cast<Move>(packed & kMoveMask);
}

int UnpackScore(std::uint64_t packed) {
	std::uint16_t score_bits = static_cast<std::uint16_t>((packed >> 32) & kScoreMask);
	return static_cast<std::int16_t>(score_bits);
}

int UnpackDepth(std::uint64_t packed) {
	std::uint8_t depth_bits = static_cast<std::uint8_t>((packed >> 48) & kDepthMask);
	return static_cast<int>(depth_bits) - kDepthBias;
}

Bound UnpackBound(std::uint64_t packed) {
	std::uint8_t bound_bits = static_cast<std::uint8_t>((packed >> 56) & kBoundMask);
	if (bound_bits > static_cast<std::uint8_t>(Bound::kUpper)) {
		bound_bits = static_cast<std::uint8_t>(Bound::kExact);
	}
	return static_cast<Bound>(bound_bits);
}

}

TranspositionTable::TranspositionTable()
	: entries_(kEntryCount),
	  mask_(kEntryCount - 1) {}

void TranspositionTable::Clear() {
	for (auto& entry : entries_) {
		entry.data.store(0, std::memory_order_relaxed);
		entry.key.store(0, std::memory_order_relaxed);
	}
}

bool TranspositionTable::Probe(std::uint64_t key, TranspositionEntry& entry) const {
	const auto& stored = entries_[key & mask_];
	std::uint64_t stored_key = stored.key.load(std::memory_order_acquire);
	if (stored_key != key) {
		return false;
	}
	std::uint64_t packed = stored.data.load(std::memory_order_relaxed);
	int depth = UnpackDepth(packed);
	if (depth < 0) {
		return false;
	}
	entry.key = key;
	entry.best_move = UnpackMove(packed);
	entry.depth = depth;
	entry.score = UnpackScore(packed);
	entry.bound = UnpackBound(packed);
	return true;
}

void TranspositionTable::Store(std::uint64_t key, int depth, int score, Bound bound,
	Move best_move) {
	auto& stored = entries_[key & mask_];
	std::uint64_t stored_key = stored.key.load(std::memory_order_relaxed);
	if (stored_key == key) {
		std::uint64_t stored_data = stored.data.load(std::memory_order_relaxed);
		int stored_depth = UnpackDepth(stored_data);
		if (stored_depth > depth) {
			return;
		}
	}
	std::uint64_t packed = PackEntry(best_move, score, depth, bound);
	stored.data.store(packed, std::memory_order_relaxed);
	stored.key.store(key, std::memory_order_release);
}

}
