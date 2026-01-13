#include "position.h"

#include "bitboard.h"
#include "zobrist.h"

namespace flare {

Position::Position() {
	Clear();
}

void Position::Clear() {
	board_.fill(Piece::kNone);
	for (auto& color_bb : piece_bb_) {
		color_bb.fill(Bitboard{0});
	}
	occupancy_bb_.fill(Bitboard{0});
	all_occupancy_bb_ = 0;
	side_to_move_ = Color::kWhite;
	castling_rights_ = 0;
	en_passant_square_ = Square::kNoSquare;
	halfmove_clock_ = 0;
	fullmove_number_ = 1;
	ComputeHash();
}

void Position::SetStartPosition() {
	Clear();

	board_[ToIndex(Square::kA1)] = Piece::kWhiteRook;
	board_[ToIndex(Square::kB1)] = Piece::kWhiteKnight;
	board_[ToIndex(Square::kC1)] = Piece::kWhiteBishop;
	board_[ToIndex(Square::kD1)] = Piece::kWhiteQueen;
	board_[ToIndex(Square::kE1)] = Piece::kWhiteKing;
	board_[ToIndex(Square::kF1)] = Piece::kWhiteBishop;
	board_[ToIndex(Square::kG1)] = Piece::kWhiteKnight;
	board_[ToIndex(Square::kH1)] = Piece::kWhiteRook;
	for (int file = 0; file < kFileCount; ++file) {
		board_[ToIndex(MakeSquare(file, 1))] = Piece::kWhitePawn;
		board_[ToIndex(MakeSquare(file, 6))] = Piece::kBlackPawn;
	}
	board_[ToIndex(Square::kA8)] = Piece::kBlackRook;
	board_[ToIndex(Square::kB8)] = Piece::kBlackKnight;
	board_[ToIndex(Square::kC8)] = Piece::kBlackBishop;
	board_[ToIndex(Square::kD8)] = Piece::kBlackQueen;
	board_[ToIndex(Square::kE8)] = Piece::kBlackKing;
	board_[ToIndex(Square::kF8)] = Piece::kBlackBishop;
	board_[ToIndex(Square::kG8)] = Piece::kBlackKnight;
	board_[ToIndex(Square::kH8)] = Piece::kBlackRook;

	castling_rights_ = kAllCastlingRights;
	side_to_move_ = Color::kWhite;
	halfmove_clock_ = 0;
	fullmove_number_ = 1;
	RebuildBitboards();
}

void Position::RebuildBitboards() {
	for (auto& color_bb : piece_bb_) {
		color_bb.fill(Bitboard{0});
	}
	occupancy_bb_.fill(Bitboard{0});
	all_occupancy_bb_ = 0;

	for (int square_index = 0; square_index < kSquareCount; ++square_index) {
		Piece piece = board_[square_index];
		if (IsNone(piece)) {
			continue;
		}
		Color color = ColorFromPiece(piece);
		PieceType type = PieceTypeFromPiece(piece);
		Bitboard bit = Bitboard{1} << square_index;
		piece_bb_[ToIndex(color)][ToIndex(type)] |= bit;
		occupancy_bb_[ToIndex(color)] |= bit;
		all_occupancy_bb_ |= bit;
	}

	ComputeHash();
}

void Position::PlacePiece(Piece piece, Square square) {
	board_[ToIndex(square)] = piece;
	if (piece == Piece::kNone) {
		return;
	}
	Color color = ColorFromPiece(piece);
	PieceType type = PieceTypeFromPiece(piece);
	Bitboard bit = SquareBit(square);
	piece_bb_[ToIndex(color)][ToIndex(type)] |= bit;
	occupancy_bb_[ToIndex(color)] |= bit;
	all_occupancy_bb_ |= bit;
}

void Position::RemovePiece(Square square) {
	Piece piece = board_[ToIndex(square)];
	if (piece == Piece::kNone) {
		return;
	}
	Color color = ColorFromPiece(piece);
	PieceType type = PieceTypeFromPiece(piece);
	Bitboard bit = SquareBit(square);
	board_[ToIndex(square)] = Piece::kNone;
	piece_bb_[ToIndex(color)][ToIndex(type)] &= ~bit;
	occupancy_bb_[ToIndex(color)] &= ~bit;
	all_occupancy_bb_ &= ~bit;
}

void Position::MovePiece(Square from, Square to) {
	Piece piece = board_[ToIndex(from)];
	RemovePiece(from);
	PlacePiece(piece, to);
}

Square Position::KingSquare(Color color) const {
	Bitboard kings = piece_bb_[ToIndex(color)][ToIndex(PieceType::kKing)];
	int index = LsbIndex(kings);
	if (index < 0) {
		return Square::kNoSquare;
	}
	return static_cast<Square>(index);
}

void Position::ComputeHash() {
	const auto& zobrist = Zobrist::Instance();
	std::uint64_t hash = 0;
	for (int square_index = 0; square_index < kSquareCount; ++square_index) {
		Piece piece = board_[square_index];
		if (piece == Piece::kNone) {
			continue;
		}
		hash ^= zobrist.PieceSquare()[ToIndex(piece)][square_index];
	}
	hash ^= zobrist.Castling()[castling_rights_];
	if (en_passant_square_ != Square::kNoSquare) {
		hash ^= zobrist.EnPassant()[FileOf(en_passant_square_)];
	}
	if (side_to_move_ == Color::kBlack) {
		hash ^= zobrist.SideToMove();
	}
	hash_ = hash;
}

}
