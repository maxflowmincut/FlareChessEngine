add_library(flare_core
	src/attack.cpp
	src/eval.cpp
	src/fen.cpp
	src/movegen.cpp
	src/perft.cpp
	src/position.cpp
	src/search.cpp
	src/transposition_table.cpp
	src/zobrist.cpp
)

target_compile_features(flare_core PUBLIC cxx_std_23)
target_include_directories(flare_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(flare_engine
	src/main.cpp
	src/uci.cpp
)
target_link_libraries(flare_engine PRIVATE flare_core)

option(FLARE_BUILD_TESTS "Build Flare Engine tests" ON)

if(FLARE_BUILD_TESTS)
	enable_testing()
	add_executable(flare_tests tests/test_main.cpp)
	target_link_libraries(flare_tests PRIVATE flare_core)
	add_test(NAME flare_tests COMMAND flare_tests)
endif()
