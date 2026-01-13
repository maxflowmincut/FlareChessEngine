#include "eval.h"

#include <array>

#include "bitboard.h"

namespace flare {
namespace {

constexpr std::array<int, kPieceTypeCount> kPieceValue = {
	0,    // kNone
	100,  // kPawn
	320,  // kKnight
	330,  // kBishop
	500,  // kRook
	900,  // kQueen
	0,    // kKing
};

constexpr std::array<int, kFileCount> kCenterFile = {0, 1, 2, 3, 3, 2, 1, 0};
constexpr std::array<int, kRankCount> kCenterRank = {0, 1, 2, 3, 3, 2, 1, 0};
constexpr std::array<int, kRankCount> kPawnRank = {0, 4, 8, 12, 16, 20, 24, 0};
constexpr std::array<int, kRankCount> kRookRank = {0, 1, 2, 2, 3, 4, 6, 0};

consteval int PstValue(PieceType type, int file, int rank) {
	switch (type) {
		case PieceType::kPawn:
			return kPawnRank[rank] + kCenterFile[file];
		case PieceType::kKnight:
			return (kCenterFile[file] + kCenterRank[rank]) * 4;
		case PieceType::kBishop:
			return (kCenterFile[file] + kCenterRank[rank]) * 3;
		case PieceType::kRook:
			return kRookRank[rank] + kCenterFile[file];
		case PieceType::kQueen:
			return (kCenterFile[file] + kCenterRank[rank]) * 2;
		case PieceType::kKing:
			return -(kCenterFile[file] + kCenterRank[rank]) * 5;
		case PieceType::kNone:
			return 0;
	}
	return 0;
}

consteval std::array<int, kSquareCount> MakePst(PieceType type) {
	std::array<int, kSquareCount> table{};
	for (int rank = 0; rank < kRankCount; ++rank) {
		for (int file = 0; file < kFileCount; ++file) {
			int index = rank * kFileCount + file;
			table[index] = PstValue(type, file, rank);
		}
	}
	return table;
}

constexpr auto kPawnPst = MakePst(PieceType::kPawn);
constexpr auto kKnightPst = MakePst(PieceType::kKnight);
constexpr auto kBishopPst = MakePst(PieceType::kBishop);
constexpr auto kRookPst = MakePst(PieceType::kRook);
constexpr auto kQueenPst = MakePst(PieceType::kQueen);
constexpr auto kKingPst = MakePst(PieceType::kKing);

const std::array<int, kSquareCount>& PstFor(PieceType type) {
	switch (type) {
		case PieceType::kPawn:
			return kPawnPst;
		case PieceType::kKnight:
			return kKnightPst;
		case PieceType::kBishop:
			return kBishopPst;
		case PieceType::kRook:
			return kRookPst;
		case PieceType::kQueen:
			return kQueenPst;
		case PieceType::kKing:
			return kKingPst;
		case PieceType::kNone:
			return kPawnPst;
	}
	return kPawnPst;
}

Square MirrorSquare(Square square) {
	int file = FileOf(square);
	int rank = RankOf(square);
	return MakeSquare(file, kRankCount - 1 - rank);
}

}

int Evaluate(const Position& position) {
	int score = 0;
	for (int color_index = 0; color_index < kColorCount; ++color_index) {
		Color color = static_cast<Color>(color_index);
		int sign = color == Color::kWhite ? 1 : -1;
		for (int type_index = ToIndex(PieceType::kPawn);
			type_index <= ToIndex(PieceType::kKing); ++type_index) {
			PieceType type = static_cast<PieceType>(type_index);
			Bitboard pieces = position.piece_bb_[color_index][type_index];
			const auto& pst = PstFor(type);
			while (pieces) {
				int square_index = PopLsb(pieces);
				Square square = static_cast<Square>(square_index);
				if (color == Color::kBlack) {
					square = MirrorSquare(square);
				}
				int value = kPieceValue[type_index];
				score += sign * (value + pst[ToIndex(square)]);
			}
		}
	}

	if (position.side_to_move_ == Color::kBlack) {
		score = -score;
	}
	return score;
}

}
