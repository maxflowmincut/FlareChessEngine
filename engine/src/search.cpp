#include "search.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <limits>
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
constexpr int kMaxPly = 64;
constexpr int kHistoryMax = 1'000'000;

struct SearchContext {
	TranspositionTable& table;
	std::uint64_t nodes = 0;
	std::array<std::array<Move, 2>, kMaxPly> killers{};
	std::array<std::array<int, kSquareCount>, kSquareCount> history{};
	std::atomic<bool>* stop = nullptr;
	std::chrono::steady_clock::time_point deadline{};
};

struct NullState {
	Square en_passant_square = Square::kNoSquare;
	Color side_to_move = Color::kWhite;
};

constexpr std::array<int, kPieceTypeCount> kMoveValues = {
	0,    // kNone
	100,  // kPawn
	320,  // kKnight
	330,  // kBishop
	500,  // kRook
	900,  // kQueen
	20000 // kKing
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

bool ShouldStop(SearchContext& context) {
	if (!context.stop) {
		return false;
	}
	if (context.stop->load(std::memory_order_relaxed)) {
		return true;
	}
	if ((context.nodes & 4095) != 0) {
		return false;
	}
	if (std::chrono::steady_clock::now() < context.deadline) {
		return false;
	}
	context.stop->store(true, std::memory_order_relaxed);
	return true;
}

bool HasNonPawnMaterial(const Position& position) {
	int us = ToIndex(position.side_to_move_);
	for (PieceType type : {PieceType::kKnight, PieceType::kBishop, PieceType::kRook,
		PieceType::kQueen}) {
		if (position.piece_bb_[us][ToIndex(type)] != 0) {
			return true;
		}
	}
	return false;
}

void MakeNullMove(Position& position, NullState& state) {
	state.en_passant_square = position.en_passant_square_;
	state.side_to_move = position.side_to_move_;
	position.en_passant_square_ = Square::kNoSquare;
	position.side_to_move_ = OppositeColor(position.side_to_move_);
	position.ComputeHash();
}

void UndoNullMove(Position& position, const NullState& state) {
	position.en_passant_square_ = state.en_passant_square;
	position.side_to_move_ = state.side_to_move;
	position.ComputeHash();
}

bool IsTacticalMove(Move move) {
	MoveFlag flag = MoveFlagOf(move);
	if (flag == MoveFlag::kPromotion || flag == MoveFlag::kEnPassant) {
		return true;
	}
	return CapturedPiece(move) != PieceType::kNone;
}

int MoveScore(Move move, Move tt_move, const SearchContext* context, int ply) {
	if (move == tt_move) {
		return 1000000;
	}
	int score = 0;
	PieceType captured = CapturedPiece(move);
	if (captured != PieceType::kNone) {
		score += 5000 + (kMoveValues[ToIndex(captured)] * 10 -
			kMoveValues[ToIndex(MovedPiece(move))]);
	}
	if (MoveFlagOf(move) == MoveFlag::kPromotion) {
		score += 8000 + kMoveValues[ToIndex(PromotionPiece(move))];
	}
	if (context && !IsTacticalMove(move)) {
		int ply_index = std::min(ply, kMaxPly - 1);
		const auto& killers = context->killers[ply_index];
		if (move == killers[0]) {
			score += 7000;
		} else if (move == killers[1]) {
			score += 6000;
		}
		score += context->history[ToIndex(FromSquare(move))][ToIndex(ToSquare(move))];
	}
	return score;
}

void OrderMoves(std::vector<Move>& moves, Move tt_move, const SearchContext* context, int ply) {
	if (moves.size() < 2) {
		return;
	}
	std::stable_sort(moves.begin(), moves.end(),
		[tt_move, context, ply](Move a, Move b) {
			return MoveScore(a, tt_move, context, ply) > MoveScore(b, tt_move, context, ply);
		});
}

void UpdateHistory(SearchContext& context, Move move, int depth, int ply) {
	if (IsTacticalMove(move)) {
		return;
	}
	int ply_index = std::min(ply, kMaxPly - 1);
	auto& killers = context.killers[ply_index];
	if (killers[0] != move) {
		killers[1] = killers[0];
		killers[0] = move;
	}
	int bonus = depth * depth;
	int& entry = context.history[ToIndex(FromSquare(move))][ToIndex(ToSquare(move))];
	entry = std::min(kHistoryMax, entry + bonus);
}

int Quiescence(Position& position, int alpha, int beta, SearchContext& context, int ply) {
	++context.nodes;
	if (ShouldStop(context)) {
		return Evaluate(position);
	}

	Square king_square = position.KingSquare(position.side_to_move_);
	bool in_check = false;
	if (king_square != Square::kNoSquare) {
		in_check = IsSquareAttacked(position, king_square,
			OppositeColor(position.side_to_move_));
	}

	int stand_pat = -kInfinity;
	if (!in_check) {
		stand_pat = Evaluate(position);
		if (stand_pat >= beta) {
			return stand_pat;
		}
		if (stand_pat > alpha) {
			alpha = stand_pat;
		}
	}

	std::vector<Move> moves;
	GenerateLegalMoves(position, moves);
	if (moves.empty()) {
		if (in_check) {
			return -kMateScore + ply;
		}
		return 0;
	}
	if (!in_check) {
		moves.erase(std::remove_if(moves.begin(), moves.end(),
			[](Move move) { return !IsTacticalMove(move); }),
			moves.end());
	}
	if (moves.empty()) {
		return stand_pat;
	}

	OrderMoves(moves, kNoMove, &context, ply);
	for (Move move : moves) {
		MoveState state;
		MakeMove(position, move, state);
		int score = -Quiescence(position, -beta, -alpha, context, ply + 1);
		UndoMove(position, move, state);

		if (score >= beta) {
			return score;
		}
		if (score > alpha) {
			alpha = score;
		}
	}
	return alpha;
}

int AlphaBeta(Position& position, int depth, int alpha, int beta, SearchContext& context,
	int ply) {
	if (depth == 0) {
		return Quiescence(position, alpha, beta, context, ply);
	}

	++context.nodes;
	if (ShouldStop(context)) {
		return Evaluate(position);
	}
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

	Square king_square = position.KingSquare(position.side_to_move_);
	bool in_check = false;
	if (king_square != Square::kNoSquare) {
		in_check = IsSquareAttacked(position, king_square,
			OppositeColor(position.side_to_move_));
	}

	if (!in_check && depth >= 3 && HasNonPawnMaterial(position)) {
		int reduction = depth >= 6 ? 3 : 2;
		NullState null_state;
		MakeNullMove(position, null_state);
		int reduced_depth = std::max(0, depth - 1 - reduction);
		int score = -AlphaBeta(position, reduced_depth, -beta, -beta + 1, context, ply + 1);
		UndoNullMove(position, null_state);
		if (score >= beta) {
			return score;
		}
	}

	std::vector<Move> moves;
	GenerateLegalMoves(position, moves);
	if (moves.empty()) {
		return in_check ? -kMateScore + ply : 0;
	}

	OrderMoves(moves, tt_move, &context, ply);

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
			UpdateHistory(context, move, depth, ply);
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

SearchResult SearchRoot(Position& position, int depth, int threads, TranspositionTable& table,
	std::atomic<bool>* stop, std::chrono::steady_clock::time_point deadline) {
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
		OrderMoves(moves, entry.best_move, nullptr, 0);
	} else {
		OrderMoves(moves, kNoMove, nullptr, 0);
	}

	int best_score = -kInfinity;
	Move best_move = kNoMove;
	std::uint64_t total_nodes = 0;

	if (threads <= 1 || moves.size() < 2) {
		SearchContext context{table};
		context.stop = stop;
		context.deadline = deadline;
		int alpha = -kInfinity;
		int beta = kInfinity;
		for (Move move : moves) {
			if (stop && stop->load(std::memory_order_relaxed)) {
				break;
			}
			MoveState state;
			MakeMove(position, move, state);
			int score = -AlphaBeta(position, depth - 1, -beta, -alpha, context, 1);
			UndoMove(position, move, state);
			if (stop && stop->load(std::memory_order_relaxed)) {
				break;
			}

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
				context.stop = stop;
				context.deadline = deadline;
				Position local = position;
				while (true) {
					if (stop && stop->load(std::memory_order_relaxed)) {
						break;
					}
					std::size_t index = next_index.fetch_add(1);
					if (index >= moves.size()) {
						break;
					}
					Move move = moves[index];
					MoveState state;
					MakeMove(local, move, state);
					int score = -AlphaBeta(local, depth - 1, -kInfinity, kInfinity, context, 1);
					UndoMove(local, move, state);
					if (stop && stop->load(std::memory_order_relaxed)) {
						break;
					}

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
	SearchLimits limits;
	limits.max_depth = max_depth;
	return Search(position, limits, table, threads);
}

SearchResult Search(Position& position, const SearchLimits& limits, TranspositionTable& table,
	int threads) {
	SearchResult result;
	SearchResult best;
	bool have_best = false;
	std::atomic<bool> local_stop{false};
	auto* stop_ptr = limits.stop;
	if (!stop_ptr && limits.time_ms > 0) {
		stop_ptr = &local_stop;
	}
	auto deadline = limits.time_ms > 0
		? std::chrono::steady_clock::now() + std::chrono::milliseconds(limits.time_ms)
		: std::chrono::steady_clock::time_point::max();
	int max_depth = limits.max_depth > 0 ? limits.max_depth : kMaxPly;
	if (limits.infinite) {
		max_depth = std::numeric_limits<int>::max();
	}
	for (int depth = 1; depth <= max_depth; ++depth) {
		if (limits.time_ms > 0 && std::chrono::steady_clock::now() >= deadline) {
			break;
		}
		result = SearchRoot(position, depth, threads, table, stop_ptr, deadline);
		if (stop_ptr && stop_ptr->load(std::memory_order_relaxed)) {
			if (!have_best) {
				best = result;
			}
			break;
		}
		best = result;
		have_best = true;
	}
	return have_best ? best : result;
}

}
