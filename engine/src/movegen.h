#pragma once

#include <vector>

#include "move.h"
#include "position.h"

namespace flare {

struct MoveState {
	Piece captured_piece_ = Piece::kNone;
	Square captured_square_ = Square::kNoSquare;
	std::uint8_t castling_rights_ = 0;
	Square en_passant_square_ = Square::kNoSquare;
	std::uint16_t halfmove_clock_ = 0;
	std::uint16_t fullmove_number_ = 1;
	Color side_to_move_ = Color::kWhite;
};

bool MakeMove(Position& position, Move move, MoveState& state);
void UndoMove(Position& position, Move move, const MoveState& state);
void GenerateLegalMoves(Position& position, std::vector<Move>& moves);

}

