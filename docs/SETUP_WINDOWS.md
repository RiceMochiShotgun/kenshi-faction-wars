# Windows setup

Everything needed to build the plugin and run it in Kenshi. Follow the steps in order: get the KenshiLib **HelloWorld** example loading in-game before building this mod, so any toolchain problem shows up on a known-good project.

This follows the [KenshiLib readme](https://github.com/KenshiReclaimer/KenshiLib) and the community [working-solution guide](https://github.com/weisspure/re_kenshi-working-solution/blob/main/README.md), which has screenshots for every Visual Studio setting below.

## 1. Kenshi and RE_Kenshi

1. Note your Kenshi version and store (Steam or GOG). RE_Kenshi supports **1.0.65**, and **1.0.68** through its installer.
2. Download the latest standard archive from [RE_Kenshi releases](https://github.com/BFrizzleFoShizzle/RE_Kenshi/releases), run `RE_Kenshi_vX.X.X.exe` and point it at your Kenshi folder.
3. Start Kenshi and check the RE_Kenshi version shows on the main menu.
4. Make a **throwaway test save**. Never test on your main playthrough.

## 2. Tools

1. **Visual Studio 2022** (Community is fine) with the *Desktop development with C++* workload. This is the full Visual Studio, not VS Code.
2. **Visual C++ 2010 x64 compilers** (the `v100` toolset). One known-working route: from a Visual Studio 2010 ISO ([Wayback Machine](https://archive.org/search?query=visual+studio+2010)), install only *Visual C++ > X64 Compilers and Tools*. Visual Studio 2022 stays your editor; it builds with `v100`.
3. **Git for Windows** with **Git LFS** (`git lfs install`). KenshiLib's `.lib` files are stored in LFS; without it you get tiny pointer files and "corrupt library" link errors.

## 3. KenshiLib dependencies

1. Clone side by side, for example:
   ```
   C:\Git\KenshiLib_Examples        https://github.com/BFrizzleFoShizzle/KenshiLib_Examples
   C:\Git\KenshiLib_Examples_deps   https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps
   ```
2. In the deps folder run `git lfs pull`.
3. Extract Boost so both of these exist (no nested `boost\boost\`, no leftover `boost.zip`):
   ```
   ...\boost_1_60_0\boost\unordered_map.hpp
   ...\boost_1_60_0\stage\lib\libboost_thread-vc100-mt-1_60.lib
   ```
4. Run the setup `.bat` from the deps package. It sets `KENSHILIB_DIR` and `BOOST_INCLUDE_PATH`. Check in a new terminal:
   ```
   echo %KENSHILIB_DIR%
   echo %BOOST_INCLUDE_PATH%
   ```
   Restart Visual Studio after any change.

## 4. Prove the chain with HelloWorld

1. Open the KenshiLib_Examples solution in Visual Studio 2022.
2. On the HelloWorld project, check: Configuration Type `Dynamic Library (.dll)`, Platform Toolset `v100`, `Release`, `x64`. If `v100` isn't in the dropdown, step 2.2 isn't installed correctly.
3. Build. The DLL lands in `x64\Release\`.
4. Create `Kenshi\mods\HelloWorld\` containing an empty `HelloWorld.mod`, `RE_Kenshi.json` with `{ "Plugins": ["HelloWorld.dll"] }`, and the DLL.
5. Enable the mod in Kenshi's launcher, start the game and look for "Hello world!" in the RE_Kenshi log.

When that works, tell Claude in the project thread. The Faction Wars Visual Studio project will copy HelloWorld's working settings.

## Common problems

| Symptom | Cause |
| --- | --- |
| `v100` missing from Platform Toolset | VC++ 2010 x64 compilers not installed |
| `LNK1104 libboost_thread-vc100-mt-1_60.lib` | Boost `stage\lib` missing, or `BOOST_INCLUDE_PATH` points at the inner `boost` folder |
| "Corrupt library" on `kenshilib.lib` | Git LFS not installed; run `git lfs pull` |
| Headers not found though paths look right | Setup `.bat` not run, or Visual Studio not restarted |
| Debug build fails | Expected; only Release x64 works |
