#pragma once

#include <array>
#include <cstdint>

#include "types.h"

namespace flare {

class Zobrist {
public:
	static const Zobrist& Instance();

	const std::array<std::array<std::uint64_t, kSquareCount>, kPieceCount>& PieceSquare() const;
	const std::array<std::uint64_t, 16>& Castling() const;
	const std::array<std::uint64_t, kFileCount>& EnPassant() const;
	std::uint64_t SideToMove() const;

private:
	Zobrist();

	std::array<std::array<std::uint64_t, kSquareCount>, kPieceCount> piece_square_{};
	std::array<std::uint64_t, 16> castling_{};
	std::array<std::uint64_t, kFileCount> en_passant_{};
	std::uint64_t side_to_move_ = 0;
};

}

