# Flare Engine

Flare Engine is a free and open source UCI chess engine, rewritten from my older C++ engine and updated for C++23 with g++-13.
It includes a local web interface in Go using WebSockets for play against the engine.
Features focus on fast move generation, parallel search, and a simple local UI.

## Build
```
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-13
cmake --build build
```

## Web Interface
Build the engine first, then run the server from `web/`.
```
go run .
```
Open `http://127.0.0.1:8080` in a browser.
To set a custom engine path or search depth:
```
go run . -engine ../build/engine/flare_engine -depth 4
```

## Bench
Command:
```
build/engine/flare_engine bench 4 1
```
Output from a local run:
```
bench startpos depth 4 score 0 nodes 2334 time_ms 17
bench kiwipete depth 4 score -10 nodes 6715 time_ms 129
bench endgame depth 4 score 5 nodes 170 time_ms 0
bench total nodes 9219 time_ms 148 nps 62290
```
