#include "perft.h"

#include <vector>

#include "movegen.h"

namespace flare {

std::uint64_t Perft(Position& position, int depth) {
	if (depth == 0) {
		return 1;
	}

	std::vector<Move> moves;
	GenerateLegalMoves(position, moves);
	std::uint64_t nodes = 0;
	for (Move move : moves) {
		MoveState state;
		MakeMove(position, move, state);
		nodes += Perft(position, depth - 1);
		UndoMove(position, move, state);
	}
	return nodes;
}

}
