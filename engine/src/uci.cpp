#include "uci.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <charconv>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "attack.h"
#include "fen.h"
#include "movegen.h"
#include "search.h"
#include "transposition_table.h"

namespace flare {
namespace {

constexpr std::string_view kStartFen =
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct UciState {
	Position position;
	TranspositionTable table;
	int threads = 1;
	int default_depth = 4;
};

std::vector<std::string> SplitTokens(const std::string& line) {
	std::istringstream input(line);
	std::vector<std::string> tokens;
	std::string token;
	while (input >> token) {
		tokens.push_back(token);
	}
	return tokens;
}

bool ParseInt(std::string_view text, int& value) {
	if (text.empty()) {
		return false;
	}
	int parsed = 0;
	auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), parsed);
	if (ec != std::errc() || ptr != text.data() + text.size()) {
		return false;
	}
	value = parsed;
	return true;
}

bool ApplyUciMove(Position& position, std::string_view uci) {
	std::vector<Move> moves;
	GenerateLegalMoves(position, moves);
	for (Move move : moves) {
		if (MoveToUci(move) == uci) {
			MoveState state;
			MakeMove(position, move, state);
			return true;
		}
	}
	return false;
}

void ApplyMoves(Position& position, const std::vector<std::string>& moves, std::size_t start) {
	for (std::size_t i = start; i < moves.size(); ++i) {
		if (!ApplyUciMove(position, moves[i])) {
			break;
		}
	}
}

bool SetPositionFromTokens(UciState& state, const std::vector<std::string>& tokens) {
	if (tokens.size() < 2) {
		return false;
	}
	if (tokens[1] == "startpos") {
		state.position.SetStartPosition();
		std::size_t moves_index = 2;
		if (moves_index < tokens.size() && tokens[moves_index] == "moves") {
			ApplyMoves(state.position, tokens, moves_index + 1);
		}
		return true;
	}
	if (tokens[1] == "fen") {
		std::string fen;
		std::size_t index = 2;
		for (; index < tokens.size(); ++index) {
			if (tokens[index] == "moves") {
				break;
			}
			if (!fen.empty()) {
				fen.push_back(' ');
			}
			fen.append(tokens[index]);
		}
		if (!LoadFen(state.position, fen)) {
			return false;
		}
		if (index < tokens.size() && tokens[index] == "moves") {
			ApplyMoves(state.position, tokens, index + 1);
		}
		return true;
	}
	return false;
}

void HandleSetOption(UciState& state, const std::vector<std::string>& tokens) {
	std::size_t name_index = 0;
	std::size_t value_index = 0;
	for (std::size_t i = 0; i < tokens.size(); ++i) {
		if (tokens[i] == "name" && i + 1 < tokens.size()) {
			name_index = i + 1;
		} else if (tokens[i] == "value" && i + 1 < tokens.size()) {
			value_index = i + 1;
		}
	}
	if (name_index == 0 || value_index == 0 || value_index <= name_index) {
		return;
	}
	std::string name;
	for (std::size_t i = name_index; i < value_index - 1; ++i) {
		if (!name.empty()) {
			name.push_back(' ');
		}
		name.append(tokens[i]);
	}
	std::string value = tokens[value_index];
	if (name == "Threads") {
		int parsed = 0;
		if (ParseInt(value, parsed)) {
			state.threads = std::max(1, parsed);
		}
	}
}

int ExtractDepth(const std::vector<std::string>& tokens, int fallback) {
	for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
		if (tokens[i] == "depth") {
			int parsed = 0;
			if (ParseInt(tokens[i + 1], parsed)) {
				return std::max(1, parsed);
			}
			return fallback;
		}
	}
	return fallback;
}

void PrintUciId(const UciState& state) {
	std::cout << "id name Flare Engine\n";
	std::cout << "id author Flare Engine\n";
	std::cout << "option name Threads type spin default " << state.threads
		<< " min 1 max 128\n";
	std::cout << "uciok\n";
}

void PrintLegalMoves(Position& position) {
	std::vector<Move> moves;
	GenerateLegalMoves(position, moves);
	std::cout << "legalmoves";
	for (Move move : moves) {
		std::cout << " " << MoveToUci(move);
	}
	std::cout << "\n";
}

void PrintFen(const Position& position) {
	std::cout << "fen " << ToFen(position) << "\n";
}

void PrintInCheck(const Position& position) {
	bool in_check = false;
	Square king_square = position.KingSquare(position.side_to_move_);
	if (king_square != Square::kNoSquare) {
		in_check = IsSquareAttacked(position, king_square,
			OppositeColor(position.side_to_move_));
	}
	std::cout << "incheck " << (in_check ? 1 : 0) << "\n";
}

}

int RunUciLoop() {
	UciState state;
	state.position.SetStartPosition();
	unsigned int hardware_threads = std::thread::hardware_concurrency();
	state.threads = hardware_threads == 0 ? 1 : static_cast<int>(hardware_threads);

	std::string line;
	while (std::getline(std::cin, line)) {
		auto tokens = SplitTokens(line);
		if (tokens.empty()) {
			continue;
		}
		const std::string& command = tokens[0];
		if (command == "uci") {
			PrintUciId(state);
		} else if (command == "isready") {
			std::cout << "readyok\n";
		} else if (command == "ucinewgame") {
			state.table.Clear();
			state.position.SetStartPosition();
		} else if (command == "setoption") {
			HandleSetOption(state, tokens);
		} else if (command == "position") {
			SetPositionFromTokens(state, tokens);
		} else if (command == "legalmoves") {
			PrintLegalMoves(state.position);
		} else if (command == "fen") {
			PrintFen(state.position);
		} else if (command == "incheck") {
			PrintInCheck(state.position);
		} else if (command == "go") {
			int depth = ExtractDepth(tokens, state.default_depth);
			SearchResult result = Search(state.position, depth, state.table, state.threads);
			std::cout << "info depth " << result.depth << " score cp " << result.score
				<< " nodes " << result.nodes << "\n";
			std::cout << "bestmove " << MoveToUci(result.best_move) << "\n";
		} else if (command == "quit") {
			break;
		}
		std::cout.flush();
	}
	return 0;
}

int RunBench(int depth, int threads) {
	std::vector<std::pair<std::string_view, std::string_view>> positions = {
		{"startpos", kStartFen},
		{"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"},
		{"endgame", "8/8/8/3k4/8/4K3/8/8 w - - 0 1"},
	};

	TranspositionTable table;
	std::uint64_t total_nodes = 0;
	auto bench_start = std::chrono::steady_clock::now();

	for (const auto& [name, fen] : positions) {
		Position position;
		if (!LoadFen(position, fen)) {
			std::cout << "bench " << name << " skipped invalid fen\n";
			continue;
		}
		auto start = std::chrono::steady_clock::now();
		SearchResult result = Search(position, depth, table, threads);
		auto end = std::chrono::steady_clock::now();
		auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		total_nodes += result.nodes;
		std::cout << "bench " << name << " depth " << depth << " score " << result.score
			<< " nodes " << result.nodes << " time_ms " << elapsed_ms.count() << "\n";
	}

	auto bench_end = std::chrono::steady_clock::now();
	auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(bench_end - bench_start);
	std::uint64_t nps = total_ms.count() == 0 ? 0 : (total_nodes * 1000 / total_ms.count());
	std::cout << "bench total nodes " << total_nodes << " time_ms " << total_ms.count()
		<< " nps " << nps << "\n";
	return 0;
}

}
