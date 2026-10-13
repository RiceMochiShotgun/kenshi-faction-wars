# Notes for working in this repo

- `core/` must never include KenshiLib, Ogre or Windows headers. It talks to the game only through the `IWorld` interface, so it compiles with VS2010 (v100) and with GCC/Clang for tests.
- Language level: plain C++03 in `core/` (CMake enforces it), even where VS2010 would accept `auto`, lambdas or `nullptr`. The plugin code may use what VS2010 supports.
- JSON: RapidJSON with `kParseCommentsFlag | kParseTrailingCommasFlag`.
- Hooks: only `KenshiLib::AddHook`. Hooks stay thin: call the original, translate types, call the core.
- Never touch MyGUI from the game thread; queue UI work for the UI thread.
- Refer to factions, towns and squads by FCS string ID.
- Plugin builds are Release x64 only (Debug is broken upstream).
- Core tests: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`. CMake holds the core to C++98 with `-pedantic -Werror` on GCC/Clang to catch anything VS2010 can't compile. Also avoid C99 math (`isfinite`, `round`, `fmin`) and `snprintf`, which VS2010 lacks.
- Don't use RapidJSON's `Value::operator[]` on a missing key; use `FindMember` (clang rejects its fallback in C++03).
- Every rule the simulation must keep belongs in `checkInvariants` (core/src/Simulation.cpp), which tests and `fw_headless` run every simulated day.
