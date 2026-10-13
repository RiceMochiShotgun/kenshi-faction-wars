# Kenshi Faction Wars

Dynamic wars between Kenshi's factions, built on Kenshi's own systems (relations, war flags, campaigns, towns) so the war is visible through vanilla mechanics and stays compatible with other mods.

**Status:** phase 4 in progress. The simulation core runs (wealth, relations, war and peace, attacks, town defense with counter-forces, saving) and is tested against a fake world. The in-game plugin isn't written yet.

## How it's built

- One C++ [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) plugin on [KenshiLib](https://github.com/KenshiReclaimer/KenshiLib).
- `core/` holds all simulation logic with **no Kenshi includes**, so it builds and is unit-tested on any compiler.
- `adapter/` and `plugin/` are the only code that touches KenshiLib.
- Game facts (factions, towns, squads) are read from FCS and never edited. Mod settings live in JSON files under `FactionWars/`, layered by mod load order.
- Battle formulas are expressions in `FactionWars/formulas.json`, so they can be tuned without recompiling.

## Layout

| Folder | What goes there |
| --- | --- |
| `core/` | Simulation: config, tick, battles, events, war state. No KenshiLib. |
| `adapter/` | `IWorld` implemented on KenshiLib |
| `plugin/` | `startPlugin()`, hook registration, threading glue |
| `tests/` | Unit tests, a fake world and the headless runner for `core/` |
| `third_party/` | RapidJSON (headers only, MIT) |
| `data/FactionWars/` | Shipped JSON settings, profiles, formulas, events |
| `data/schema/` | JSON Schemas for editor autocomplete |
| `package/FactionWars/` | Files copied into `Kenshi/mods/FactionWars/` |
| `tools/` | Deploy and validation scripts |
| `docs/` | Setup and design notes |

## Building

The core and its tests build anywhere with CMake:

```
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
./build/fw_headless --years 10        # 10 in-game years on a sample world, with rule checks
```

`fw_headless` reads `data/FactionWars/` (or `--data DIR`), so it's the quickest way to try changes to `formulas.json` or `settings.json`.

Plugin builds need Windows, Visual Studio 2022 and the Visual C++ 2010 x64 (`v100`) compilers. See [docs/SETUP_WINDOWS.md](docs/SETUP_WINDOWS.md).

## Licence

GPLv3, as required by KenshiLib. See [LICENSE](LICENSE).
