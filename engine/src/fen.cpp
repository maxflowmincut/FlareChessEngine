#include "fen.h"

#include <charconv>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace flare {
namespace {

Piece PieceFromFen(char piece) {
	switch (piece) {
		case 'P':
			return Piece::kWhitePawn;
		case 'N':
			return Piece::kWhiteKnight;
		case 'B':
			return Piece::kWhiteBishop;
		case 'R':
			return Piece::kWhiteRook;
		case 'Q':
			return Piece::kWhiteQueen;
		case 'K':
			return Piece::kWhiteKing;
		case 'p':
			return Piece::kBlackPawn;
		case 'n':
			return Piece::kBlackKnight;
		case 'b':
			return Piece::kBlackBishop;
		case 'r':
			return Piece::kBlackRook;
		case 'q':
			return Piece::kBlackQueen;
		case 'k':
			return Piece::kBlackKing;
		default:
			return Piece::kNone;
	}
}

char PieceToFenChar(Piece piece) {
	switch (piece) {
		case Piece::kWhitePawn:
			return 'P';
		case Piece::kWhiteKnight:
			return 'N';
		case Piece::kWhiteBishop:
			return 'B';
		case Piece::kWhiteRook:
			return 'R';
		case Piece::kWhiteQueen:
			return 'Q';
		case Piece::kWhiteKing:
			return 'K';
		case Piece::kBlackPawn:
			return 'p';
		case Piece::kBlackKnight:
			return 'n';
		case Piece::kBlackBishop:
			return 'b';
		case Piece::kBlackRook:
			return 'r';
		case Piece::kBlackQueen:
			return 'q';
		case Piece::kBlackKing:
			return 'k';
		case Piece::kNone:
			return '\0';
	}
	return '\0';
}

bool ParseInt(std::string_view value, int& output) {
	if (value.empty()) {
		return false;
	}
	int result = 0;
	auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
	if (ec != std::errc() || ptr != value.data() + value.size()) {
		return false;
	}
	output = result;
	return true;
}

std::vector<std::string_view> SplitFields(std::string_view fen) {
	std::vector<std::string_view> fields;
	std::size_t pos = 0;
	while (pos < fen.size()) {
		while (pos < fen.size() && std::isspace(static_cast<unsigned char>(fen[pos]))) {
			++pos;
		}
		if (pos >= fen.size()) {
			break;
		}
		std::size_t start = pos;
		while (pos < fen.size() && !std::isspace(static_cast<unsigned char>(fen[pos]))) {
			++pos;
		}
		fields.push_back(fen.substr(start, pos - start));
	}
	return fields;
}

}

bool LoadFen(Position& position, std::string_view fen) {
	auto fields = SplitFields(fen);
	if (fields.size() < 4) {
		return false;
	}

	position.Clear();

	int rank = kRankCount - 1;
	int file = 0;
	for (char ch : fields[0]) {
		if (ch == '/') {
			if (file != kFileCount) {
				return false;
			}
			--rank;
			file = 0;
			continue;
		}
		if (std::isdigit(static_cast<unsigned char>(ch))) {
			if (ch < '1' || ch > '8') {
				return false;
			}
			file += ch - '0';
			if (file > kFileCount) {
				return false;
			}
			continue;
		}
		Piece piece = PieceFromFen(ch);
		if (piece == Piece::kNone) {
			return false;
		}
		if (rank < 0 || file >= kFileCount) {
			return false;
		}
		position.board_[ToIndex(MakeSquare(file, rank))] = piece;
		++file;
	}
	if (rank != 0 || file != kFileCount) {
		return false;
	}

	if (fields[1] == "w") {
		position.side_to_move_ = Color::kWhite;
	} else if (fields[1] == "b") {
		position.side_to_move_ = Color::kBlack;
	} else {
		return false;
	}

	position.castling_rights_ = 0;
	if (fields[2] != "-") {
		for (char ch : fields[2]) {
			switch (ch) {
				case 'K':
					position.castling_rights_ |= kWhiteKingSideCastle;
					break;
				case 'Q':
					position.castling_rights_ |= kWhiteQueenSideCastle;
					break;
				case 'k':
					position.castling_rights_ |= kBlackKingSideCastle;
					break;
				case 'q':
					position.castling_rights_ |= kBlackQueenSideCastle;
					break;
				default:
					return false;
			}
		}
	}

	position.en_passant_square_ = Square::kNoSquare;
	if (fields[3] != "-") {
		if (fields[3].size() != 2) {
			return false;
		}
		char file_char = fields[3][0];
		char rank_char = fields[3][1];
		if (file_char < 'a' || file_char > 'h' || rank_char < '1' || rank_char > '8') {
			return false;
		}
		int ep_file = file_char - 'a';
		int ep_rank = rank_char - '1';
		position.en_passant_square_ = MakeSquare(ep_file, ep_rank);
	}

	position.halfmove_clock_ = 0;
	position.fullmove_number_ = 1;
	if (fields.size() >= 5) {
		int halfmove = 0;
		if (!ParseInt(fields[4], halfmove)) {
			return false;
		}
		position.halfmove_clock_ = static_cast<std::uint16_t>(halfmove);
	}
	if (fields.size() >= 6) {
		int fullmove = 0;
		if (!ParseInt(fields[5], fullmove)) {
			return false;
		}
		position.fullmove_number_ = static_cast<std::uint16_t>(fullmove);
	}

	position.RebuildBitboards();
	return true;
}

std::string ToFen(const Position& position) {
	std::string fen;
	fen.reserve(80);

	for (int rank = kRankCount - 1; rank >= 0; --rank) {
		int empty = 0;
		for (int file = 0; file < kFileCount; ++file) {
			Piece piece = position.board_[ToIndex(MakeSquare(file, rank))];
			if (piece == Piece::kNone) {
				++empty;
				continue;
			}
			if (empty > 0) {
				fen.push_back(static_cast<char>('0' + empty));
				empty = 0;
			}
			char piece_char = PieceToFenChar(piece);
			if (piece_char != '\0') {
				fen.push_back(piece_char);
			}
		}
		if (empty > 0) {
			fen.push_back(static_cast<char>('0' + empty));
		}
		if (rank > 0) {
			fen.push_back('/');
		}
	}

	fen.push_back(' ');
	fen.push_back(position.side_to_move_ == Color::kWhite ? 'w' : 'b');
	fen.push_back(' ');

	std::string castling;
	if (position.castling_rights_ & kWhiteKingSideCastle) {
		castling.push_back('K');
	}
	if (position.castling_rights_ & kWhiteQueenSideCastle) {
		castling.push_back('Q');
	}
	if (position.castling_rights_ & kBlackKingSideCastle) {
		castling.push_back('k');
	}
	if (position.castling_rights_ & kBlackQueenSideCastle) {
		castling.push_back('q');
	}
	if (castling.empty()) {
		fen.push_back('-');
	} else {
		fen.append(castling);
	}

	fen.push_back(' ');
	if (position.en_passant_square_ == Square::kNoSquare) {
		fen.push_back('-');
	} else {
		int file = FileOf(position.en_passant_square_);
		int rank = RankOf(position.en_passant_square_);
		fen.push_back(static_cast<char>('a' + file));
		fen.push_back(static_cast<char>('1' + rank));
	}

	fen.push_back(' ');
	fen.append(std::to_string(position.halfmove_clock_));
	fen.push_back(' ');
	fen.append(std::to_string(position.fullmove_number_));
	return fen;
}

}
