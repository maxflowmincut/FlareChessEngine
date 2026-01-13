#include "attack.h"

#include <array>

namespace flare {
namespace {

constexpr std::array<std::array<int, 2>, 8> kKnightOffsets = {{
	{1, 2}, {2, 1}, {2, -1}, {1, -2},
	{-1, -2}, {-2, -1}, {-2, 1}, {-1, 2},
}};

constexpr std::array<std::array<int, 2>, 8> kKingOffsets = {{
	{1, 1}, {1, 0}, {1, -1}, {0, -1},
	{-1, -1}, {-1, 0}, {-1, 1}, {0, 1},
}};

Bitboard RayAttacks(Square square, Bitboard occupancy, int file_delta, int rank_delta) {
	int file = FileOf(square);
	int rank = RankOf(square);
	Bitboard attacks = 0;
	file += file_delta;
	rank += rank_delta;
	while (file >= 0 && file < kFileCount && rank >= 0 && rank < kRankCount) {
		Square target = MakeSquare(file, rank);
		Bitboard bit = SquareBit(target);
		attacks |= bit;
		if (occupancy & bit) {
			break;
		}
		file += file_delta;
		rank += rank_delta;
	}
	return attacks;
}

}

Bitboard PawnAttacks(Color color, Square square) {
	int file = FileOf(square);
	int rank = RankOf(square);
	int forward = color == Color::kWhite ? 1 : -1;
	int target_rank = rank + forward;
	if (target_rank < 0 || target_rank >= kRankCount) {
		return 0;
	}
	Bitboard attacks = 0;
	if (file > 0) {
		attacks |= SquareBit(MakeSquare(file - 1, target_rank));
	}
	if (file < kFileCount - 1) {
		attacks |= SquareBit(MakeSquare(file + 1, target_rank));
	}
	return attacks;
}

Bitboard KnightAttacks(Square square) {
	int file = FileOf(square);
	int rank = RankOf(square);
	Bitboard attacks = 0;
	for (const auto& offset : kKnightOffsets) {
		int target_file = file + offset[0];
		int target_rank = rank + offset[1];
		if (target_file < 0 || target_file >= kFileCount || target_rank < 0 ||
			target_rank >= kRankCount) {
			continue;
		}
		attacks |= SquareBit(MakeSquare(target_file, target_rank));
	}
	return attacks;
}

Bitboard KingAttacks(Square square) {
	int file = FileOf(square);
	int rank = RankOf(square);
	Bitboard attacks = 0;
	for (const auto& offset : kKingOffsets) {
		int target_file = file + offset[0];
		int target_rank = rank + offset[1];
		if (target_file < 0 || target_file >= kFileCount || target_rank < 0 ||
			target_rank >= kRankCount) {
			continue;
		}
		attacks |= SquareBit(MakeSquare(target_file, target_rank));
	}
	return attacks;
}

Bitboard BishopAttacks(Square square, Bitboard occupancy) {
	return RayAttacks(square, occupancy, 1, 1) |
		RayAttacks(square, occupancy, 1, -1) |
		RayAttacks(square, occupancy, -1, 1) |
		RayAttacks(square, occupancy, -1, -1);
}

Bitboard RookAttacks(Square square, Bitboard occupancy) {
	return RayAttacks(square, occupancy, 1, 0) |
		RayAttacks(square, occupancy, -1, 0) |
		RayAttacks(square, occupancy, 0, 1) |
		RayAttacks(square, occupancy, 0, -1);
}

Bitboard QueenAttacks(Square square, Bitboard occupancy) {
	return BishopAttacks(square, occupancy) | RookAttacks(square, occupancy);
}

bool IsSquareAttacked(const Position& position, Square square, Color by_color) {
	Bitboard occupancy = position.all_occupancy_bb_;
	Bitboard pawns = position.piece_bb_[ToIndex(by_color)][ToIndex(PieceType::kPawn)];
	Bitboard knights = position.piece_bb_[ToIndex(by_color)][ToIndex(PieceType::kKnight)];
	Bitboard bishops = position.piece_bb_[ToIndex(by_color)][ToIndex(PieceType::kBishop)];
	Bitboard rooks = position.piece_bb_[ToIndex(by_color)][ToIndex(PieceType::kRook)];
	Bitboard queens = position.piece_bb_[ToIndex(by_color)][ToIndex(PieceType::kQueen)];
	Bitboard kings = position.piece_bb_[ToIndex(by_color)][ToIndex(PieceType::kKing)];

	if (PawnAttacks(OppositeColor(by_color), square) & pawns) {
		return true;
	}
	if (KnightAttacks(square) & knights) {
		return true;
	}
	if (KingAttacks(square) & kings) {
		return true;
	}
	if (BishopAttacks(square, occupancy) & (bishops | queens)) {
		return true;
	}
	if (RookAttacks(square, occupancy) & (rooks | queens)) {
		return true;
	}
	return false;
}

}
