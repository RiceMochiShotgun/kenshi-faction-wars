Tests for `core/`, built with CMake and a modern compiler (the core itself stays C++03).

- `support/`: a tiny test framework, `FakeWorld` (an in-memory `IWorld`), a vanilla-like sample world, and a helper that loads a `FactionWars/` folder.
- `unit/`: unit tests (`fw_tests`).
- `headless/`: `fw_headless`, which runs the simulation on the sample world for years of in-game time, checks every rule each day and prints a summary. Used for tuning `formulas.json`.

```
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
./build/fw_headless --years 10 --seed 7
```
