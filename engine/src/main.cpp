#include <algorithm>
#include <cstdlib>
#include <string_view>
#include <thread>

#include "uci.h"

int main(int argc, char* argv[]) {
	if (argc > 1 && std::string_view(argv[1]) == "bench") {
		int depth = 5;
		int threads = 1;
		if (argc > 2) {
			depth = std::max(1, std::atoi(argv[2]));
		}
		if (argc > 3) {
			threads = std::max(1, std::atoi(argv[3]));
		} else {
			unsigned int hardware_threads = std::thread::hardware_concurrency();
			threads = hardware_threads == 0 ? 1 : static_cast<int>(hardware_threads);
		}
		return flare::RunBench(depth, threads);
	}
	return flare::RunUciLoop();
}
