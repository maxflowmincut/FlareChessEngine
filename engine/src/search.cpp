#include "search.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "attack.h"
#include "eval.h"
#include "movegen.h"

namespace flare {
namespace {

constexpr int kInfinity = 32000;
constexpr int kMateScore = 30000;
constexpr int kMateThreshold = 29000;

struct SearchContext {
	TranspositionTable& table;
	std::uint64_t nodes = 0;
};

int ScoreToTt(int score, int ply) {
	if (score > kMateThreshold) {
		return score + ply;
	}
	if (score < -kMateThreshold) {
		return score - ply;
	}
	return score;
}

int ScoreFromTt(int score, int ply) {
	if (score > kMateThreshold) {
		return score - ply;
	}
	if (score < -kMateThreshold) {
		return score + ply;
	}
	return score;
}

int AlphaBeta(Position& position, int depth, int alpha, int beta, SearchContext& context,
	int ply) {
	++context.nodes;

	int alpha_orig = alpha;
	int beta_orig = beta;
	std::uint64_t key = position.hash_;
	Move tt_move = kNoMove;
	TranspositionEntry entry;

	if (context.table.Probe(key, entry)) {
		tt_move = entry.best_move;
		if (entry.depth >= depth) {
			int tt_score = ScoreFromTt(entry.score, ply);
			if (entry.bound == Bound::kExact) {
				return tt_score;
			}
			if (entry.bound == Bound::kLower) {
				alpha = std::max(alpha, tt_score);
			} else if (entry.bound == Bound::kUpper) {
				beta = std::min(beta, tt_score);
			}
			if (alpha >= beta) {
				return tt_score;
			}
		}
	}

	if (depth == 0) {
		return Evaluate(position);
	}

	std::vector<Move> moves;
	GenerateLegalMoves(position, moves);
	if (moves.empty()) {
		Square king_square = position.KingSquare(position.side_to_move_);
		bool in_check = IsSquareAttacked(position, king_square,
			OppositeColor(position.side_to_move_));
		return in_check ? -kMateScore + ply : 0;
	}

	if (tt_move != kNoMove) {
		auto it = std::find(moves.begin(), moves.end(), tt_move);
		if (it != moves.end()) {
			std::iter_swap(moves.begin(), it);
		}
	}

	Move best_move = kNoMove;
	int best_score = -kInfinity;

	for (Move move : moves) {
		MoveState state;
		MakeMove(position, move, state);
		int score = -AlphaBeta(position, depth - 1, -beta, -alpha, context, ply + 1);
		UndoMove(position, move, state);

		if (score > best_score) {
			best_score = score;
			best_move = move;
		}
		if (score > alpha) {
			alpha = score;
		}
		if (alpha >= beta) {
			break;
		}
	}

	Bound bound;
	if (best_score <= alpha_orig) {
		bound = Bound::kUpper;
	} else if (best_score >= beta_orig) {
		bound = Bound::kLower;
	} else {
		bound = Bound::kExact;
	}
	context.table.Store(key, depth, ScoreToTt(best_score, ply), bound, best_move);
	return best_score;
}

SearchResult SearchRoot(Position& position, int depth, int threads, TranspositionTable& table) {
	SearchResult result;
	std::vector<Move> moves;
	GenerateLegalMoves(position, moves);

	if (moves.empty()) {
		Square king_square = position.KingSquare(position.side_to_move_);
		bool in_check = IsSquareAttacked(position, king_square,
			OppositeColor(position.side_to_move_));
		result.score = in_check ? -kMateScore : 0;
		result.depth = depth;
		return result;
	}

	TranspositionEntry entry;
	if (table.Probe(position.hash_, entry)) {
		if (entry.best_move != kNoMove) {
			auto it = std::find(moves.begin(), moves.end(), entry.best_move);
			if (it != moves.end()) {
				std::iter_swap(moves.begin(), it);
			}
		}
	}

	int best_score = -kInfinity;
	Move best_move = kNoMove;
	std::uint64_t total_nodes = 0;

	if (threads <= 1 || moves.size() < 2) {
		SearchContext context{table};
		int alpha = -kInfinity;
		int beta = kInfinity;
		for (Move move : moves) {
			MoveState state;
			MakeMove(position, move, state);
			int score = -AlphaBeta(position, depth - 1, -beta, -alpha, context, 1);
			UndoMove(position, move, state);

			if (score > best_score) {
				best_score = score;
				best_move = move;
			}
			alpha = std::max(alpha, score);
			if (alpha >= beta) {
				break;
			}
		}
		total_nodes = context.nodes;
	} else {
		std::atomic<std::size_t> next_index{0};
		std::mutex best_mutex;
		std::vector<std::uint64_t> nodes_per_thread(static_cast<std::size_t>(threads), 0);
		std::vector<std::thread> workers;
		workers.reserve(static_cast<std::size_t>(threads));

		for (int thread_index = 0; thread_index < threads; ++thread_index) {
			workers.emplace_back([&, thread_index]() {
				SearchContext context{table};
				Position local = position;
				while (true) {
					std::size_t index = next_index.fetch_add(1);
					if (index >= moves.size()) {
						break;
					}
					Move move = moves[index];
					MoveState state;
					MakeMove(local, move, state);
					int score = -AlphaBeta(local, depth - 1, -kInfinity, kInfinity, context, 1);
					UndoMove(local, move, state);

					{
						std::lock_guard<std::mutex> guard(best_mutex);
						if (score > best_score) {
							best_score = score;
							best_move = move;
						}
					}
				}
				nodes_per_thread[static_cast<std::size_t>(thread_index)] = context.nodes;
			});
		}

		for (auto& worker : workers) {
			worker.join();
		}

		for (std::uint64_t nodes : nodes_per_thread) {
			total_nodes += nodes;
		}
	}

	result.best_move = best_move;
	result.score = best_score;
	result.depth = depth;
	result.nodes = total_nodes;
	table.Store(position.hash_, depth, ScoreToTt(best_score, 0), Bound::kExact, best_move);
	return result;
}

}

SearchResult Search(Position& position, int max_depth, TranspositionTable& table, int threads) {
	SearchResult result;
	for (int depth = 1; depth <= max_depth; ++depth) {
		result = SearchRoot(position, depth, threads, table);
	}
	return result;
}

}
