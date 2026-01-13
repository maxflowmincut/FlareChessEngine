#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "fen.h"
#include "movegen.h"
#include "perft.h"
#include "position.h"

namespace flare {

static int g_failures = 0;

struct JsonTestCase {
	std::string start_fen;
	std::vector<std::string> expected_fens;
};

class JsonParser {
public:
	explicit JsonParser(std::string_view input) : input_(input) {}

	bool Consume(char expected) {
		SkipWhitespace();
		if (pos_ < input_.size() && input_[pos_] == expected) {
			++pos_;
			return true;
		}
		return false;
	}

	bool Peek(char expected) {
		SkipWhitespace();
		return pos_ < input_.size() && input_[pos_] == expected;
	}

	std::string ParseString() {
		SkipWhitespace();
		if (pos_ >= input_.size() || input_[pos_] != '"') {
			return {};
		}
		++pos_;
		std::string output;
		while (pos_ < input_.size()) {
			char ch = input_[pos_++];
			if (ch == '"') {
				break;
			}
			if (ch == '\\' && pos_ < input_.size()) {
				char esc = input_[pos_++];
				switch (esc) {
					case '"':
						output.push_back('"');
						break;
					case '\\':
						output.push_back('\\');
						break;
					case '/':
						output.push_back('/');
						break;
					case 'b':
						output.push_back('\b');
						break;
					case 'f':
						output.push_back('\f');
						break;
					case 'n':
						output.push_back('\n');
						break;
					case 'r':
						output.push_back('\r');
						break;
					case 't':
						output.push_back('\t');
						break;
					case 'u':
						for (int i = 0; i < 4 && pos_ < input_.size(); ++i) {
							++pos_;
						}
						break;
					default:
						output.push_back(esc);
						break;
				}
			} else {
				output.push_back(ch);
			}
		}
		return output;
	}

	void SkipValue() {
		SkipWhitespace();
		if (pos_ >= input_.size()) {
			return;
		}
		char ch = input_[pos_];
		if (ch == '"') {
			ParseString();
			return;
		}
		if (ch == '{') {
			SkipObject();
			return;
		}
		if (ch == '[') {
			SkipArray();
			return;
		}
		if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '-') {
			SkipNumber();
			return;
		}
		while (pos_ < input_.size() && std::isalpha(static_cast<unsigned char>(input_[pos_]))) {
			++pos_;
		}
	}

private:
	void SkipWhitespace() {
		while (pos_ < input_.size() &&
			std::isspace(static_cast<unsigned char>(input_[pos_]))) {
			++pos_;
		}
	}

	void SkipObject() {
		if (!Consume('{')) {
			return;
		}
		if (Consume('}')) {
			return;
		}
		while (pos_ < input_.size()) {
			ParseString();
			Consume(':');
			SkipValue();
			if (Consume('}')) {
				return;
			}
			Consume(',');
		}
	}

	void SkipArray() {
		if (!Consume('[')) {
			return;
		}
		if (Consume(']')) {
			return;
		}
		while (pos_ < input_.size()) {
			SkipValue();
			if (Consume(']')) {
				return;
			}
			Consume(',');
		}
	}

	void SkipNumber() {
		if (pos_ < input_.size() && input_[pos_] == '-') {
			++pos_;
		}
		while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
			++pos_;
		}
		if (pos_ < input_.size() && input_[pos_] == '.') {
			++pos_;
			while (pos_ < input_.size() &&
				std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
				++pos_;
			}
		}
	}

	std::string_view input_;
	std::size_t pos_ = 0;
};

void Expect(bool condition, std::string_view message) {
	if (!condition) {
		++g_failures;
		std::cerr << "FAIL: " << message << "\n";
	}
}

void ExpectEqual(std::uint64_t actual, std::uint64_t expected, std::string_view message) {
	if (actual != expected) {
		++g_failures;
		std::cerr << "FAIL: " << message << " expected " << expected << " got " << actual
			<< "\n";
	}
}

bool HasPawnEnPassantCapture(const Position& position) {
	if (position.en_passant_square_ == Square::kNoSquare) {
		return false;
	}
	int file = FileOf(position.en_passant_square_);
	int rank = RankOf(position.en_passant_square_);
	int pawn_rank = rank + (position.side_to_move_ == Color::kWhite ? -1 : 1);
	if (pawn_rank < 0 || pawn_rank >= kRankCount) {
		return false;
	}
	for (int df : {-1, 1}) {
		int target_file = file + df;
		if (target_file < 0 || target_file >= kFileCount) {
			continue;
		}
		Square source = MakeSquare(target_file, pawn_rank);
		Piece piece = position.board_[ToIndex(source)];
		if (piece == MakePiece(position.side_to_move_, PieceType::kPawn)) {
			return true;
		}
	}
	return false;
}

std::string NormalizeTestFen(std::string_view fen) {
	Position position;
	if (!LoadFen(position, fen)) {
		return std::string(fen);
	}
	if (position.en_passant_square_ == Square::kNoSquare) {
		return std::string(fen);
	}
	if (HasPawnEnPassantCapture(position)) {
		return std::string(fen);
	}
	position.en_passant_square_ = Square::kNoSquare;
	return ToFen(position);
}

bool ParseJsonTestFile(std::string_view json, std::vector<JsonTestCase>& cases) {
	JsonParser parser(json);
	if (!parser.Consume('{')) {
		return false;
	}
	while (!parser.Peek('}')) {
		std::string key = parser.ParseString();
		if (key.empty()) {
			return false;
		}
		if (!parser.Consume(':')) {
			return false;
		}
		if (key == "testCases") {
			if (!parser.Consume('[')) {
				return false;
			}
			if (parser.Consume(']')) {
				break;
			}
			while (true) {
				JsonTestCase test_case;
				if (!parser.Consume('{')) {
					return false;
				}
				while (!parser.Peek('}')) {
					std::string case_key = parser.ParseString();
					if (case_key.empty()) {
						return false;
					}
					if (!parser.Consume(':')) {
						return false;
					}
					if (case_key == "start") {
						if (!parser.Consume('{')) {
							return false;
						}
						while (!parser.Peek('}')) {
							std::string start_key = parser.ParseString();
							if (!parser.Consume(':')) {
								return false;
							}
							if (start_key == "fen") {
								test_case.start_fen = parser.ParseString();
							} else {
								parser.SkipValue();
							}
							if (parser.Consume('}')) {
								break;
							}
							parser.Consume(',');
						}
					} else if (case_key == "expected") {
						if (!parser.Consume('[')) {
							return false;
						}
						if (!parser.Peek(']')) {
							while (true) {
								if (!parser.Consume('{')) {
									return false;
								}
								while (!parser.Peek('}')) {
									std::string expected_key = parser.ParseString();
									if (!parser.Consume(':')) {
										return false;
									}
									if (expected_key == "fen") {
										test_case.expected_fens.push_back(parser.ParseString());
									} else {
										parser.SkipValue();
									}
									if (parser.Consume('}')) {
										break;
									}
									parser.Consume(',');
								}
								if (parser.Consume(']')) {
									break;
								}
								parser.Consume(',');
							}
						} else {
							parser.Consume(']');
						}
					} else {
						parser.SkipValue();
					}
					if (parser.Consume('}')) {
						break;
					}
					parser.Consume(',');
				}
				cases.push_back(std::move(test_case));
				if (parser.Consume(']')) {
					break;
				}
				parser.Consume(',');
			}
		} else {
			parser.SkipValue();
		}
		if (parser.Consume('}')) {
			break;
		}
		parser.Consume(',');
	}
	return true;
}

bool HasJsonFiles(const std::filesystem::path& dir) {
	if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
		return false;
	}
	for (const auto& entry : std::filesystem::directory_iterator(dir)) {
		if (entry.is_regular_file() && entry.path().extension() == ".json") {
			return true;
		}
	}
	return false;
}

std::filesystem::path FindJsonTestcasesDir() {
	namespace fs = std::filesystem;
	std::vector<fs::path> roots = {
		fs::current_path(),
		fs::current_path() / "..",
		fs::current_path() / ".." / "..",
	};
	for (const auto& root : roots) {
		fs::path candidate = root / "src" / "main" / "resources" / "testcases";
		if (HasJsonFiles(candidate)) {
			return candidate;
		}
		candidate = root / "testcases";
		if (HasJsonFiles(candidate)) {
			return candidate;
		}
		if (!fs::exists(root) || !fs::is_directory(root)) {
			continue;
		}
		for (const auto& entry : fs::directory_iterator(root)) {
			if (!entry.is_directory()) {
				continue;
			}
			fs::path nested = entry.path() / "src" / "main" / "resources" / "testcases";
			if (HasJsonFiles(nested)) {
				return nested;
			}
			nested = entry.path() / "testcases";
			if (HasJsonFiles(nested)) {
				return nested;
			}
		}
	}
	return {};
}

void TestJsonTestcases() {
	namespace fs = std::filesystem;
	fs::path testcases_dir = FindJsonTestcasesDir();
	if (testcases_dir.empty()) {
		std::cerr << "json movegen testcases not found, skipping\n";
		return;
	}

	int total_cases = 0;
	bool printed_detail = false;
	for (const auto& entry : fs::directory_iterator(testcases_dir)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".json") {
			continue;
		}
		std::ifstream input(entry.path());
		if (!input) {
			Expect(false, "json testcase read");
			continue;
		}
		std::string contents((std::istreambuf_iterator<char>(input)),
			std::istreambuf_iterator<char>());
		std::vector<JsonTestCase> cases;
		if (!ParseJsonTestFile(contents, cases)) {
			Expect(false, "json testcase parse");
			continue;
		}
		int case_index = 0;
		for (const auto& test_case : cases) {
			++case_index;
			++total_cases;
			Position position;
			if (!LoadFen(position, test_case.start_fen)) {
				Expect(false, "json testcase fen parse");
				continue;
			}
			std::string start_fen = ToFen(position);
			std::vector<Move> moves;
			GenerateLegalMoves(position, moves);
			std::unordered_set<std::string> actual_fens;
			actual_fens.reserve(moves.size());
			for (Move move : moves) {
				MoveState state;
				MakeMove(position, move, state);
				actual_fens.insert(NormalizeTestFen(ToFen(position)));
				UndoMove(position, move, state);
				if (ToFen(position) != start_fen) {
					Expect(false, "json testcase undo mismatch");
					break;
				}
			}
			std::unordered_set<std::string> expected_fens;
			expected_fens.reserve(test_case.expected_fens.size());
			for (const auto& fen : test_case.expected_fens) {
				expected_fens.insert(NormalizeTestFen(fen));
			}
			std::string case_id = entry.path().filename().string() + ":" +
				std::to_string(case_index);
			if (actual_fens.size() != expected_fens.size()) {
				Expect(false, case_id + " size mismatch");
			}
			int missing_count = 0;
			int unexpected_count = 0;
			for (const auto& expected : expected_fens) {
				if (!actual_fens.contains(expected)) {
					Expect(false, case_id + " missing expected fen");
					++missing_count;
					if (!printed_detail && missing_count <= 3) {
						std::cerr << "missing " << expected << "\n";
					}
				}
			}
			for (const auto& actual : actual_fens) {
				if (!expected_fens.contains(actual)) {
					Expect(false, case_id + " unexpected fen");
					++unexpected_count;
					if (!printed_detail && unexpected_count <= 3) {
						std::cerr << "unexpected " << actual << "\n";
					}
				}
			}
			if (!printed_detail && (missing_count > 0 || unexpected_count > 0)) {
				printed_detail = true;
				std::cerr << "testcase mismatch " << case_id << "\n";
				std::cerr << "start " << test_case.start_fen << "\n";
			}
		}
	}
	if (total_cases == 0) {
		Expect(false, "json testcases found");
	}
}

void DumpPerftDivide(Position& position, int depth) {
	std::vector<Move> moves;
	GenerateLegalMoves(position, moves);
	for (Move move : moves) {
		MoveState state;
		MakeMove(position, move, state);
		std::uint64_t nodes = Perft(position, depth - 1);
		UndoMove(position, move, state);
		std::cerr << MoveToUci(move) << " " << nodes << "\n";
	}
}

void TestStartPositionPerft() {
	Position position;
	position.SetStartPosition();
	ExpectEqual(Perft(position, 1), 20, "startpos perft depth 1");
	ExpectEqual(Perft(position, 2), 400, "startpos perft depth 2");
	ExpectEqual(Perft(position, 3), 8902, "startpos perft depth 3");
}

void TestKiwipetePerft() {
	Position position;
	bool ok = LoadFen(position,
		"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
	Expect(ok, "kiwipete fen parse");
	if (!ok) {
		return;
	}
	std::vector<Move> moves;
	GenerateLegalMoves(position, moves);
	bool has_queenside_castle = false;
	for (Move move : moves) {
		if (MoveFlagOf(move) == MoveFlag::kCastle &&
			FromSquare(move) == Square::kE1 &&
			ToSquare(move) == Square::kC1) {
			has_queenside_castle = true;
			break;
		}
	}
	Expect(has_queenside_castle, "kiwipete queenside castle available");
	std::uint64_t depth1 = Perft(position, 1);
	std::uint64_t depth2 = Perft(position, 2);
	ExpectEqual(depth1, 48, "kiwipete perft depth 1");
	ExpectEqual(depth2, 2039, "kiwipete perft depth 2");
	if (depth1 != 48 || depth2 != 2039) {
		DumpPerftDivide(position, 2);
	}
}

void TestEnPassant() {
	Position position;
	bool ok = LoadFen(position, "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
	Expect(ok, "en passant fen parse");
	if (!ok) {
		return;
	}

	std::vector<Move> moves;
	GenerateLegalMoves(position, moves);
	Move ep_move = kNoMove;
	for (Move move : moves) {
		if (MoveFlagOf(move) == MoveFlag::kEnPassant) {
			ep_move = move;
			break;
		}
	}

	Expect(ep_move != kNoMove, "en passant move generated");
	if (ep_move == kNoMove) {
		return;
	}

	MoveState state;
	MakeMove(position, ep_move, state);
	Expect(position.board_[ToIndex(Square::kD5)] == Piece::kNone,
		"en passant capture removes pawn");
	Expect(position.board_[ToIndex(Square::kD6)] == Piece::kWhitePawn,
		"en passant capture places pawn");
	UndoMove(position, ep_move, state);
	Expect(position.board_[ToIndex(Square::kD5)] == Piece::kBlackPawn,
		"undo en passant restores pawn");
}

void TestEnPassantTargetSquare() {
	Position position;
	bool ok = LoadFen(position, "4k3/8/8/8/3p4/8/4P3/4K3 w - - 0 1");
	Expect(ok, "en passant target fen parse");
	if (ok) {
		std::vector<Move> moves;
		GenerateLegalMoves(position, moves);
		Move double_push = kNoMove;
		for (Move move : moves) {
			if (MoveFlagOf(move) == MoveFlag::kDoublePush &&
				FromSquare(move) == Square::kE2 &&
				ToSquare(move) == Square::kE4) {
				double_push = move;
				break;
			}
		}
		Expect(double_push != kNoMove, "double pawn move available");
		if (double_push != kNoMove) {
			MoveState state;
			MakeMove(position, double_push, state);
			Expect(position.en_passant_square_ == Square::kE3,
				"en passant target set when capture possible");
			UndoMove(position, double_push, state);
			Expect(position.en_passant_square_ == Square::kNoSquare,
				"en passant target restored after undo");
		}
	}

	ok = LoadFen(position, "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
	Expect(ok, "en passant target empty fen parse");
	if (ok) {
		std::vector<Move> moves;
		GenerateLegalMoves(position, moves);
		Move double_push = kNoMove;
		for (Move move : moves) {
			if (MoveFlagOf(move) == MoveFlag::kDoublePush &&
				FromSquare(move) == Square::kE2 &&
				ToSquare(move) == Square::kE4) {
				double_push = move;
				break;
			}
		}
		Expect(double_push != kNoMove, "double pawn move available no capture");
		if (double_push != kNoMove) {
			MoveState state;
			MakeMove(position, double_push, state);
			Expect(position.en_passant_square_ == Square::kNoSquare,
				"no en passant target when capture impossible");
			UndoMove(position, double_push, state);
			Expect(position.en_passant_square_ == Square::kNoSquare,
				"en passant target restored after undo no capture");
		}
	}
}

void TestCastlingPerft() {
	Position position;
	bool ok = LoadFen(position, "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
	Expect(ok, "castling fen parse");
	if (!ok) {
		return;
	}
	ExpectEqual(Perft(position, 1), 26, "castling perft depth 1");
	ExpectEqual(Perft(position, 2), 568, "castling perft depth 2");
}

void TestPromotionMoves() {
	Position position;
	bool ok = LoadFen(position, "7k/P7/8/8/8/8/7p/7K w - - 0 1");
	Expect(ok, "promotion fen parse");
	if (!ok) {
		return;
	}

	std::vector<Move> moves;
	GenerateLegalMoves(position, moves);
	int promotion_moves = 0;
	for (Move move : moves) {
		if (MoveFlagOf(move) == MoveFlag::kPromotion) {
			++promotion_moves;
		}
	}
	ExpectEqual(promotion_moves, 4, "promotion move count");
}

void RunTests() {
	TestStartPositionPerft();
	TestKiwipetePerft();
	TestEnPassant();
	TestEnPassantTargetSquare();
	TestCastlingPerft();
	TestPromotionMoves();
	TestJsonTestcases();
}

int Failures() {
	return g_failures;
}

}

int main() {
	flare::RunTests();

	if (flare::Failures() == 0) {
		std::cout << "All tests passed.\n";
		return 0;
	}
	std::cerr << flare::Failures() << " tests failed.\n";
	return 1;
}
