#pragma once

#include "position.h"

namespace flare {

Bitboard PawnAttacks(Color color, Square square);
Bitboard KnightAttacks(Square square);
Bitboard KingAttacks(Square square);
Bitboard BishopAttacks(Square square, Bitboard occupancy);
Bitboard RookAttacks(Square square, Bitboard occupancy);
Bitboard QueenAttacks(Square square, Bitboard occupancy);
bool IsSquareAttacked(const Position& position, Square square, Color by_color);

}

