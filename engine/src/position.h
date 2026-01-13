#pragma once

#include <array>

#include "types.h"

namespace flare {

struct Position {
	Position();

	void Clear();
	void SetStartPosition();
	void RebuildBitboards();
	void PlacePiece(Piece piece, Square square);
	void RemovePiece(Square square);
	void MovePiece(Square from, Square to);
	Square KingSquare(Color color) const;
	void ComputeHash();

	std::array<Piece, kSquareCount> board_{};
	std::array<std::array<Bitboard, kPieceTypeCount>, kColorCount> piece_bb_{};
	std::array<Bitboard, kColorCount> occupancy_bb_{};
	Bitboard all_occupancy_bb_ = 0;
	Color side_to_move_ = Color::kWhite;
	std::uint8_t castling_rights_ = 0;
	Square en_passant_square_ = Square::kNoSquare;
	std::uint16_t halfmove_clock_ = 0;
	std::uint16_t fullmove_number_ = 1;
	std::uint64_t hash_ = 0;
};

}

