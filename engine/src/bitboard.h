#pragma once

#include <bit>

#include "types.h"

namespace flare {

inline int PopLsb(Bitboard& bitboard) {
	if (bitboard == 0) {
		return -1;
	}
	int index = std::countr_zero(bitboard);
	bitboard &= bitboard - 1;
	return index;
}

inline int LsbIndex(Bitboard bitboard) {
	if (bitboard == 0) {
		return -1;
	}
	return std::countr_zero(bitboard);
}

inline bool HasBit(Bitboard bitboard, Square square) {
	return (bitboard & SquareBit(square)) != 0;
}

}

