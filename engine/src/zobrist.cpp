#include "zobrist.h"

namespace flare {
namespace {

// Deterministic mixer for Zobrist keys so hashes are stable across runs.
std::uint64_t NextRandom(std::uint64_t& state) {
	state += 0x9e3779b97f4a7c15ULL;
	std::uint64_t result = state;
	result = (result ^ (result >> 30)) * 0xbf58476d1ce4e5b9ULL;
	result = (result ^ (result >> 27)) * 0x94d049bb133111ebULL;
	return result ^ (result >> 31);
}

}

Zobrist::Zobrist() {
	std::uint64_t state = 0x853c49e6748fea9bULL;
	for (auto& piece_entries : piece_square_) {
		for (auto& entry : piece_entries) {
			entry = NextRandom(state);
		}
	}
	for (auto& entry : castling_) {
		entry = NextRandom(state);
	}
	for (auto& entry : en_passant_) {
		entry = NextRandom(state);
	}
	side_to_move_ = NextRandom(state);
}

const Zobrist& Zobrist::Instance() {
	static Zobrist instance;
	return instance;
}

const std::array<std::array<std::uint64_t, kSquareCount>, kPieceCount>&
Zobrist::PieceSquare() const {
	return piece_square_;
}

const std::array<std::uint64_t, 16>& Zobrist::Castling() const {
	return castling_;
}

const std::array<std::uint64_t, kFileCount>& Zobrist::EnPassant() const {
	return en_passant_;
}

std::uint64_t Zobrist::SideToMove() const {
	return side_to_move_;
}

}
