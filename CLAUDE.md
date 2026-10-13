# Notes for working in this repo

- `core/` must never include KenshiLib, Ogre or Windows headers. It talks to the game only through the `IWorld` interface, so it compiles with VS2010 (v100) and with GCC/Clang for tests.
- Language level: C++03 plus what VS2010 supports (`auto`, lambdas, `nullptr`, `static_assert`). No range-for, `enum class`, variadic templates, `std::thread`, initializer lists or `= default`.
- JSON: RapidJSON with `kParseCommentsFlag | kParseTrailingCommasFlag`.
- Hooks: only `KenshiLib::AddHook`. Hooks stay thin: call the original, translate types, call the core.
- Never touch MyGUI from the game thread; queue UI work for the UI thread.
- Refer to factions, towns and squads by FCS string ID.
- Plugin builds are Release x64 only (Debug is broken upstream).
