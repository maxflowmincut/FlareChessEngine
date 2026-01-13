#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace flare {

using Bitboard = std::uint64_t;

enum class Color : std::uint8_t {
	kWhite = 0,
	kBlack = 1,
};

constexpr int kColorCount = 2;

enum class PieceType : std::uint8_t {
	kNone = 0,
	kPawn,
	kKnight,
	kBishop,
	kRook,
	kQueen,
	kKing,
};

constexpr int kPieceTypeCount = 7;

enum class Piece : std::uint8_t {
	kNone = 0,
	kWhitePawn,
	kWhiteKnight,
	kWhiteBishop,
	kWhiteRook,
	kWhiteQueen,
	kWhiteKing,
	kBlackPawn,
	kBlackKnight,
	kBlackBishop,
	kBlackRook,
	kBlackQueen,
	kBlackKing,
};

constexpr int kPieceCount = 13;

enum class Square : std::uint8_t {
	kA1 = 0,
	kB1,
	kC1,
	kD1,
	kE1,
	kF1,
	kG1,
	kH1,
	kA2,
	kB2,
	kC2,
	kD2,
	kE2,
	kF2,
	kG2,
	kH2,
	kA3,
	kB3,
	kC3,
	kD3,
	kE3,
	kF3,
	kG3,
	kH3,
	kA4,
	kB4,
	kC4,
	kD4,
	kE4,
	kF4,
	kG4,
	kH4,
	kA5,
	kB5,
	kC5,
	kD5,
	kE5,
	kF5,
	kG5,
	kH5,
	kA6,
	kB6,
	kC6,
	kD6,
	kE6,
	kF6,
	kG6,
	kH6,
	kA7,
	kB7,
	kC7,
	kD7,
	kE7,
	kF7,
	kG7,
	kH7,
	kA8,
	kB8,
	kC8,
	kD8,
	kE8,
	kF8,
	kG8,
	kH8,
	kNoSquare = 64,
};

constexpr int kSquareCount = 64;
constexpr int kFileCount = 8;
constexpr int kRankCount = 8;

constexpr std::uint8_t kWhiteKingSideCastle = 1 << 0;
constexpr std::uint8_t kWhiteQueenSideCastle = 1 << 1;
constexpr std::uint8_t kBlackKingSideCastle = 1 << 2;
constexpr std::uint8_t kBlackQueenSideCastle = 1 << 3;
constexpr std::uint8_t kAllCastlingRights = kWhiteKingSideCastle | kWhiteQueenSideCastle |
	kBlackKingSideCastle | kBlackQueenSideCastle;

constexpr int ToIndex(Color color) {
	return static_cast<int>(color);
}

constexpr int ToIndex(PieceType type) {
	return static_cast<int>(type);
}

constexpr int ToIndex(Piece piece) {
	return static_cast<int>(piece);
}

constexpr int ToIndex(Square square) {
	return static_cast<int>(square);
}

constexpr Color OppositeColor(Color color) {
	return color == Color::kWhite ? Color::kBlack : Color::kWhite;
}

constexpr Bitboard SquareBit(Square square) {
	return Bitboard{1} << ToIndex(square);
}

constexpr int FileOf(Square square) {
	return ToIndex(square) % kFileCount;
}

constexpr int RankOf(Square square) {
	return ToIndex(square) / kFileCount;
}

constexpr Square MakeSquare(int file, int rank) {
	return static_cast<Square>(rank * kFileCount + file);
}

constexpr bool IsNone(Piece piece) {
	return piece == Piece::kNone;
}

constexpr Color ColorFromPiece(Piece piece) {
	return ToIndex(piece) >= ToIndex(Piece::kBlackPawn) ? Color::kBlack : Color::kWhite;
}

constexpr PieceType PieceTypeFromPiece(Piece piece) {
	if (piece == Piece::kNone) {
		return PieceType::kNone;
	}
	int offset = (ToIndex(piece) - 1) % 6;
	return static_cast<PieceType>(offset + 1);
}

constexpr Piece MakePiece(Color color, PieceType type) {
	if (type == PieceType::kNone) {
		return Piece::kNone;
	}
	int base = color == Color::kWhite ? ToIndex(Piece::kWhitePawn) : ToIndex(Piece::kBlackPawn);
	int offset = ToIndex(type) - 1;
	return static_cast<Piece>(base + offset);
}

inline constexpr std::array<std::string_view, kSquareCount> kSquareNames = {
	"a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
	"a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
	"a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
	"a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
	"a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
	"a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
	"a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
	"a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};

}

