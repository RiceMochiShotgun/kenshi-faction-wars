Simulation core. No KenshiLib, Ogre or Windows includes; it talks to the game only through `IWorld` (`include/fw/World.h`), so it builds with Visual C++ 2010 for the plugin and with GCC/Clang/MSVC for tests.

| File | What it does |
| --- | --- |
| `World.h` | `IWorld`: the core's only view of the game (factions, towns, squads, relations, war flags, campaigns, notifications) |
| `Config.h`, `ConfigLoader.cpp` | Builds settings, faction profiles, town importance, squad costs and events from auto defaults plus layered JSON files |
| `Expr.h/.cpp` | The small expression language used by `formulas.json` |
| `Formulas.h/.cpp` | Named formulas with their inputs, built-in defaults and weights |
| `WarState.h/.cpp` | Everything saved with the game, and its JSON save format |
| `Simulation.h/.cpp` | The daily tick: met factions, battle results, wealth, relation drift, war and peace, attack planning, squad cap and queue; plus `checkInvariants` |
| `Log.h/.cpp` | Messages for `FactionWars.log` |
| `Random.h` | Deterministic RNG whose state is saved, so a reloaded save continues the same way |

Not built yet (later phases): defense counter-forces, joining battles, vultures, town capture, the world event engine.
