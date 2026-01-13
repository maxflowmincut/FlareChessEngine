#include "movegen.h"

#include <array>

#include "attack.h"
#include "bitboard.h"

namespace flare {
namespace {

constexpr std::array<PieceType, 4> kPromotionTypes = {
	PieceType::kQueen,
	PieceType::kRook,
	PieceType::kBishop,
	PieceType::kKnight,
};

void AddMove(std::vector<Move>& moves, Square from, Square to, PieceType piece, PieceType capture,
	PieceType promotion, MoveFlag flag) {
	moves.push_back(EncodeMove(from, to, piece, capture, promotion, flag));
}

void AddPromotionMoves(std::vector<Move>& moves, Square from, Square to, PieceType capture) {
	for (PieceType promotion : kPromotionTypes) {
		AddMove(moves, from, to, PieceType::kPawn, capture, promotion, MoveFlag::kPromotion);
	}
}

void GeneratePawnMoves(Position& position, std::vector<Move>& moves, Color color) {
	int color_index = ToIndex(color);
	int forward = color == Color::kWhite ? 1 : -1;
	int start_rank = color == Color::kWhite ? 1 : 6;
	int promotion_rank = color == Color::kWhite ? 6 : 1;

	Bitboard pawns = position.piece_bb_[color_index][ToIndex(PieceType::kPawn)];
	while (pawns) {
		int from_index = PopLsb(pawns);
		Square from = static_cast<Square>(from_index);
		int file = FileOf(from);
		int rank = RankOf(from);
		int next_rank = rank + forward;
		if (next_rank >= 0 && next_rank < kRankCount) {
			Square one_step = MakeSquare(file, next_rank);
			if (!HasBit(position.all_occupancy_bb_, one_step)) {
				if (rank == promotion_rank) {
					AddPromotionMoves(moves, from, one_step, PieceType::kNone);
				} else {
					AddMove(moves, from, one_step, PieceType::kPawn, PieceType::kNone,
						PieceType::kNone, MoveFlag::kNone);
					if (rank == start_rank) {
						int double_rank = rank + (2 * forward);
						Square two_step = MakeSquare(file, double_rank);
						if (!HasBit(position.all_occupancy_bb_, two_step)) {
							AddMove(moves, from, two_step, PieceType::kPawn, PieceType::kNone,
								PieceType::kNone, MoveFlag::kDoublePush);
						}
					}
				}
			}
			for (int file_offset : {-1, 1}) {
				int target_file = file + file_offset;
				if (target_file < 0 || target_file >= kFileCount) {
					continue;
				}
				Square target = MakeSquare(target_file, next_rank);
				Piece target_piece = position.board_[ToIndex(target)];
				if (target_piece != Piece::kNone &&
					ColorFromPiece(target_piece) == OppositeColor(color)) {
					PieceType capture = PieceTypeFromPiece(target_piece);
					if (rank == promotion_rank) {
						AddPromotionMoves(moves, from, target, capture);
					} else {
						AddMove(moves, from, target, PieceType::kPawn, capture, PieceType::kNone,
							MoveFlag::kNone);
					}
				}
				if (position.en_passant_square_ == target) {
					AddMove(moves, from, target, PieceType::kPawn, PieceType::kPawn,
						PieceType::kNone, MoveFlag::kEnPassant);
				}
			}
		}
	}
}

void GenerateKnightMoves(Position& position, std::vector<Move>& moves, Color color) {
	int color_index = ToIndex(color);
	Bitboard knights = position.piece_bb_[color_index][ToIndex(PieceType::kKnight)];
	Bitboard own_occ = position.occupancy_bb_[color_index];
	while (knights) {
		int from_index = PopLsb(knights);
		Square from = static_cast<Square>(from_index);
		Bitboard attacks = KnightAttacks(from) & ~own_occ;
		while (attacks) {
			int to_index = PopLsb(attacks);
			Square to = static_cast<Square>(to_index);
			Piece target_piece = position.board_[to_index];
			PieceType capture = PieceType::kNone;
			if (target_piece != Piece::kNone && ColorFromPiece(target_piece) != color) {
				capture = PieceTypeFromPiece(target_piece);
			}
			AddMove(moves, from, to, PieceType::kKnight, capture, PieceType::kNone,
				MoveFlag::kNone);
		}
	}
}

void GenerateSlidingMoves(Position& position, std::vector<Move>& moves, Color color,
	PieceType piece_type) {
	int color_index = ToIndex(color);
	Bitboard own_occ = position.occupancy_bb_[color_index];
	Bitboard pieces = position.piece_bb_[color_index][ToIndex(piece_type)];
	while (pieces) {
		int from_index = PopLsb(pieces);
		Square from = static_cast<Square>(from_index);
		Bitboard attacks = 0;
		if (piece_type == PieceType::kBishop) {
			attacks = BishopAttacks(from, position.all_occupancy_bb_);
		} else if (piece_type == PieceType::kRook) {
			attacks = RookAttacks(from, position.all_occupancy_bb_);
		} else if (piece_type == PieceType::kQueen) {
			attacks = QueenAttacks(from, position.all_occupancy_bb_);
		}
		attacks &= ~own_occ;
		while (attacks) {
			int to_index = PopLsb(attacks);
			Square to = static_cast<Square>(to_index);
			Piece target_piece = position.board_[to_index];
			PieceType capture = PieceType::kNone;
			if (target_piece != Piece::kNone && ColorFromPiece(target_piece) != color) {
				capture = PieceTypeFromPiece(target_piece);
			}
			AddMove(moves, from, to, piece_type, capture, PieceType::kNone, MoveFlag::kNone);
		}
	}
}

void GenerateKingMoves(Position& position, std::vector<Move>& moves, Color color) {
	int color_index = ToIndex(color);
	Bitboard own_occ = position.occupancy_bb_[color_index];
	Square king_square = position.KingSquare(color);
	if (king_square == Square::kNoSquare) {
		return;
	}

	Bitboard attacks = KingAttacks(king_square) & ~own_occ;
	while (attacks) {
		int to_index = PopLsb(attacks);
		Square to = static_cast<Square>(to_index);
		Piece target_piece = position.board_[to_index];
		PieceType capture = PieceType::kNone;
		if (target_piece != Piece::kNone && ColorFromPiece(target_piece) != color) {
			capture = PieceTypeFromPiece(target_piece);
		}
		AddMove(moves, king_square, to, PieceType::kKing, capture, PieceType::kNone,
			MoveFlag::kNone);
	}

	Color enemy = OppositeColor(color);
	bool in_check = IsSquareAttacked(position, king_square, enemy);
	if (in_check) {
		return;
	}

	if (color == Color::kWhite && king_square == Square::kE1) {
		if (position.castling_rights_ & kWhiteKingSideCastle) {
			if (position.board_[ToIndex(Square::kF1)] == Piece::kNone &&
				position.board_[ToIndex(Square::kG1)] == Piece::kNone &&
				position.board_[ToIndex(Square::kH1)] == Piece::kWhiteRook &&
				!IsSquareAttacked(position, Square::kF1, enemy) &&
				!IsSquareAttacked(position, Square::kG1, enemy)) {
				AddMove(moves, Square::kE1, Square::kG1, PieceType::kKing, PieceType::kNone,
					PieceType::kNone, MoveFlag::kCastle);
			}
		}
		if (position.castling_rights_ & kWhiteQueenSideCastle) {
			if (position.board_[ToIndex(Square::kD1)] == Piece::kNone &&
				position.board_[ToIndex(Square::kC1)] == Piece::kNone &&
				position.board_[ToIndex(Square::kB1)] == Piece::kNone &&
				position.board_[ToIndex(Square::kA1)] == Piece::kWhiteRook &&
				!IsSquareAttacked(position, Square::kD1, enemy) &&
				!IsSquareAttacked(position, Square::kC1, enemy)) {
				AddMove(moves, Square::kE1, Square::kC1, PieceType::kKing, PieceType::kNone,
					PieceType::kNone, MoveFlag::kCastle);
			}
		}
	} else if (color == Color::kBlack && king_square == Square::kE8) {
		if (position.castling_rights_ & kBlackKingSideCastle) {
			if (position.board_[ToIndex(Square::kF8)] == Piece::kNone &&
				position.board_[ToIndex(Square::kG8)] == Piece::kNone &&
				position.board_[ToIndex(Square::kH8)] == Piece::kBlackRook &&
				!IsSquareAttacked(position, Square::kF8, enemy) &&
				!IsSquareAttacked(position, Square::kG8, enemy)) {
				AddMove(moves, Square::kE8, Square::kG8, PieceType::kKing, PieceType::kNone,
					PieceType::kNone, MoveFlag::kCastle);
			}
		}
		if (position.castling_rights_ & kBlackQueenSideCastle) {
			if (position.board_[ToIndex(Square::kD8)] == Piece::kNone &&
				position.board_[ToIndex(Square::kC8)] == Piece::kNone &&
				position.board_[ToIndex(Square::kB8)] == Piece::kNone &&
				position.board_[ToIndex(Square::kA8)] == Piece::kBlackRook &&
				!IsSquareAttacked(position, Square::kD8, enemy) &&
				!IsSquareAttacked(position, Square::kC8, enemy)) {
				AddMove(moves, Square::kE8, Square::kC8, PieceType::kKing, PieceType::kNone,
					PieceType::kNone, MoveFlag::kCastle);
			}
		}
	}
}

void GeneratePseudoLegalMoves(Position& position, std::vector<Move>& moves) {
	moves.clear();
	Color color = position.side_to_move_;
	GeneratePawnMoves(position, moves, color);
	GenerateKnightMoves(position, moves, color);
	GenerateSlidingMoves(position, moves, color, PieceType::kBishop);
	GenerateSlidingMoves(position, moves, color, PieceType::kRook);
	GenerateSlidingMoves(position, moves, color, PieceType::kQueen);
	GenerateKingMoves(position, moves, color);
}

void UpdateCastlingRights(Position& position, Square from, Square to, Piece moved_piece,
	Piece captured_piece, Square captured_square) {
	if (moved_piece == Piece::kWhiteKing) {
		position.castling_rights_ &= ~(kWhiteKingSideCastle | kWhiteQueenSideCastle);
	} else if (moved_piece == Piece::kBlackKing) {
		position.castling_rights_ &= ~(kBlackKingSideCastle | kBlackQueenSideCastle);
	} else if (moved_piece == Piece::kWhiteRook) {
		if (from == Square::kA1) {
			position.castling_rights_ &= ~kWhiteQueenSideCastle;
		} else if (from == Square::kH1) {
			position.castling_rights_ &= ~kWhiteKingSideCastle;
		}
	} else if (moved_piece == Piece::kBlackRook) {
		if (from == Square::kA8) {
			position.castling_rights_ &= ~kBlackQueenSideCastle;
		} else if (from == Square::kH8) {
			position.castling_rights_ &= ~kBlackKingSideCastle;
		}
	}

	if (captured_piece == Piece::kWhiteRook) {
		if (captured_square == Square::kA1) {
			position.castling_rights_ &= ~kWhiteQueenSideCastle;
		} else if (captured_square == Square::kH1) {
			position.castling_rights_ &= ~kWhiteKingSideCastle;
		}
	} else if (captured_piece == Piece::kBlackRook) {
		if (captured_square == Square::kA8) {
			position.castling_rights_ &= ~kBlackQueenSideCastle;
		} else if (captured_square == Square::kH8) {
			position.castling_rights_ &= ~kBlackKingSideCastle;
		}
	}
}

}

bool MakeMove(Position& position, Move move, MoveState& state) {
	Square from = FromSquare(move);
	Square to = ToSquare(move);
	MoveFlag flag = MoveFlagOf(move);
	PieceType moved_type = MovedPiece(move);
	Piece moved_piece = position.board_[ToIndex(from)];
	Color us = position.side_to_move_;

	state.captured_piece_ = Piece::kNone;
	state.captured_square_ = Square::kNoSquare;
	state.castling_rights_ = position.castling_rights_;
	state.en_passant_square_ = position.en_passant_square_;
	state.halfmove_clock_ = position.halfmove_clock_;
	state.fullmove_number_ = position.fullmove_number_;
	state.side_to_move_ = position.side_to_move_;

	position.en_passant_square_ = Square::kNoSquare;

	if (flag == MoveFlag::kEnPassant) {
		int capture_rank = RankOf(to) + (us == Color::kWhite ? -1 : 1);
		state.captured_square_ = MakeSquare(FileOf(to), capture_rank);
		state.captured_piece_ = position.board_[ToIndex(state.captured_square_)];
		position.RemovePiece(state.captured_square_);
	} else if (position.board_[ToIndex(to)] != Piece::kNone) {
		state.captured_square_ = to;
		state.captured_piece_ = position.board_[ToIndex(to)];
		position.RemovePiece(to);
	}

	if (flag == MoveFlag::kPromotion) {
		position.RemovePiece(from);
		Piece promotion_piece = MakePiece(us, PromotionPiece(move));
		position.PlacePiece(promotion_piece, to);
	} else {
		position.MovePiece(from, to);
	}

	if (flag == MoveFlag::kCastle) {
		if (us == Color::kWhite) {
			if (to == Square::kG1) {
				position.MovePiece(Square::kH1, Square::kF1);
			} else if (to == Square::kC1) {
				position.MovePiece(Square::kA1, Square::kD1);
			}
		} else {
			if (to == Square::kG8) {
				position.MovePiece(Square::kH8, Square::kF8);
			} else if (to == Square::kC8) {
				position.MovePiece(Square::kA8, Square::kD8);
			}
		}
	}

	UpdateCastlingRights(position, from, to, moved_piece, state.captured_piece_,
		state.captured_square_);

	if (flag == MoveFlag::kDoublePush) {
		int passed_rank = RankOf(from) + (us == Color::kWhite ? 1 : -1);
		Square ep_square = MakeSquare(FileOf(from), passed_rank);
		Color enemy = OppositeColor(us);
		Bitboard enemy_pawns = position.piece_bb_[ToIndex(enemy)][ToIndex(PieceType::kPawn)];
		if (PawnAttacks(OppositeColor(enemy), ep_square) & enemy_pawns) {
			position.en_passant_square_ = ep_square;
		}
	}

	bool is_capture = state.captured_piece_ != Piece::kNone;
	if (moved_type == PieceType::kPawn || is_capture) {
		position.halfmove_clock_ = 0;
	} else {
		++position.halfmove_clock_;
	}

	if (us == Color::kBlack) {
		++position.fullmove_number_;
	}

	position.side_to_move_ = OppositeColor(position.side_to_move_);
	position.ComputeHash();
	return true;
}

void UndoMove(Position& position, Move move, const MoveState& state) {
	position.side_to_move_ = state.side_to_move_;
	position.castling_rights_ = state.castling_rights_;
	position.en_passant_square_ = state.en_passant_square_;
	position.halfmove_clock_ = state.halfmove_clock_;
	position.fullmove_number_ = state.fullmove_number_;

	Square from = FromSquare(move);
	Square to = ToSquare(move);
	MoveFlag flag = MoveFlagOf(move);

	if (flag == MoveFlag::kPromotion) {
		position.RemovePiece(to);
		position.PlacePiece(MakePiece(position.side_to_move_, PieceType::kPawn), from);
	} else {
		position.MovePiece(to, from);
	}

	if (flag == MoveFlag::kCastle) {
		if (position.side_to_move_ == Color::kWhite) {
			if (to == Square::kG1) {
				position.MovePiece(Square::kF1, Square::kH1);
			} else if (to == Square::kC1) {
				position.MovePiece(Square::kD1, Square::kA1);
			}
		} else {
			if (to == Square::kG8) {
				position.MovePiece(Square::kF8, Square::kH8);
			} else if (to == Square::kC8) {
				position.MovePiece(Square::kD8, Square::kA8);
			}
		}
	}

	if (state.captured_piece_ != Piece::kNone) {
		position.PlacePiece(state.captured_piece_, state.captured_square_);
	}

	position.ComputeHash();
}

void GenerateLegalMoves(Position& position, std::vector<Move>& moves) {
	std::vector<Move> pseudo_moves;
	GeneratePseudoLegalMoves(position, pseudo_moves);

	moves.clear();
	Color us = position.side_to_move_;
	Color them = OppositeColor(us);
	for (Move move : pseudo_moves) {
		if (CapturedPiece(move) == PieceType::kKing) {
			continue;
		}
		MoveState state;
		MakeMove(position, move, state);
		Square king_square = position.KingSquare(us);
		if (king_square != Square::kNoSquare &&
			!IsSquareAttacked(position, king_square, them)) {
			moves.push_back(move);
		}
		UndoMove(position, move, state);
	}
}

}
