#pragma once

#include <cstdint>
#include <string>

#include "types.h"

namespace flare {

using Move = std::uint32_t;

enum class MoveFlag : std::uint8_t {
	kNone = 0,
	kPromotion = 1,
	kEnPassant = 2,
	kCastle = 3,
	kDoublePush = 4,
};

constexpr Move kNoMove = 0;

constexpr int ToIndex(MoveFlag flag) {
	return static_cast<int>(flag);
}

constexpr std::uint32_t kFromShift = 0;
constexpr std::uint32_t kToShift = 6;
constexpr std::uint32_t kPieceShift = 12;
constexpr std::uint32_t kCaptureShift = 16;
constexpr std::uint32_t kPromotionShift = 20;
constexpr std::uint32_t kFlagShift = 24;

constexpr std::uint32_t kSquareMask = 0x3F;
constexpr std::uint32_t kPieceMask = 0xF;
constexpr std::uint32_t kFlagMask = 0xF;

constexpr Move EncodeMove(Square from, Square to, PieceType piece, PieceType capture,
	PieceType promotion, MoveFlag flag) {
	return (static_cast<Move>(ToIndex(from)) << kFromShift) |
		(static_cast<Move>(ToIndex(to)) << kToShift) |
		(static_cast<Move>(ToIndex(piece)) << kPieceShift) |
		(static_cast<Move>(ToIndex(capture)) << kCaptureShift) |
		(static_cast<Move>(ToIndex(promotion)) << kPromotionShift) |
		(static_cast<Move>(ToIndex(flag)) << kFlagShift);
}

constexpr Square FromSquare(Move move) {
	return static_cast<Square>((move >> kFromShift) & kSquareMask);
}

constexpr Square ToSquare(Move move) {
	return static_cast<Square>((move >> kToShift) & kSquareMask);
}

constexpr PieceType MovedPiece(Move move) {
	return static_cast<PieceType>((move >> kPieceShift) & kPieceMask);
}

constexpr PieceType CapturedPiece(Move move) {
	return static_cast<PieceType>((move >> kCaptureShift) & kPieceMask);
}

constexpr PieceType PromotionPiece(Move move) {
	return static_cast<PieceType>((move >> kPromotionShift) & kPieceMask);
}

constexpr MoveFlag MoveFlagOf(Move move) {
	return static_cast<MoveFlag>((move >> kFlagShift) & kFlagMask);
}

inline char PromotionChar(PieceType type) {
	switch (type) {
		case PieceType::kKnight:
			return 'n';
		case PieceType::kBishop:
			return 'b';
		case PieceType::kRook:
			return 'r';
		case PieceType::kQueen:
			return 'q';
		default:
			return '\0';
	}
}

inline std::string MoveToUci(Move move) {
	if (move == kNoMove) {
		return "0000";
	}
	Square from = FromSquare(move);
	Square to = ToSquare(move);
	std::string uci(kSquareNames[ToIndex(from)]);
	uci.append(kSquareNames[ToIndex(to)]);
	if (MoveFlagOf(move) == MoveFlag::kPromotion) {
		char promotion = PromotionChar(PromotionPiece(move));
		if (promotion != '\0') {
			uci.push_back(promotion);
		}
	}
	return uci;
}

}

