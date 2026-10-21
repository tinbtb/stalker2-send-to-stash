# Send to your stash

Native ASI test build for S.T.A.L.K.E.R. 2. Adds **Send to your stash** to the existing inventory right-click menu, from anywhere. No UE4SS dependency and no replacement game assets.

The implementation uses the personal-stash container identified from the premium-edition Journalist flash-drive reward, then calls the game's existing item-transfer/stack-selection path. It only offers the action for carried items whose native Drop permission allows it and whose Unequip action is not visible. Save/reload persistence and preservation of item state still require in-game verification.

## Current test

Supported executable SHA-256: `61bc1e030740cebc30cf1dad0c86cf65e39e12ff0500225821d684181e08d56b`. Different executables or mismatching native entry bytes disable the plugin before hooks are installed.

`build/SendToStash.asi` is the test artifact. `tools/install_test.ps1` checks the installed executable, requires an existing recognized ASI loader, and copies only this plugin. It backs up an earlier copy and does not replace proxy DLLs. The plugin writes `SendToStash.log` next to itself. Restart the game after installation or removal; do not unload it during play.

To uninstall this test, close the game and remove only `SendToStash.asi` from `Stalker2/Binaries/Win64` (or restore the prior-copy backup listed in `build/installation.json`). Leave the existing loader and other plugins alone.

Test on a manual save: right-click one ordinary carried item, send it, and confirm it disappears from inventory and appears in a base's personal stash. Then check a partial stack with the quantity selector and cancellation. Finally verify another stash location and save/reload. Weapon condition, upgrades, attachments, and loaded ammunition need explicit comparison before calling this ready for release.

## Build and evidence

GitHub Actions builds `SendToStash.asi` on Windows using the same script, Zig 0.15.2, vendored MinHook, and checked-in game profile. Each push, pull request, or manual run produces a `SendToStash-windows-x64` artifact with the ASI, SHA-256 checksum, DLL audit, README, and MinHook license. Download it from the run's Artifacts section. This creates test artifacts, not tagged releases.

CI checks the DLL exports/imports and runs the unsupported-host guard. It does not require or distribute the game executable, and does not test gameplay. Debug information includes build paths, so the CI ASI is not guaranteed byte-identical to a local build. The menu label is currently English-only, without a localization SID.

Run `tools/build_native.ps1 -Zig <zig.exe>` with Zig 0.15.2. The MinHook C sources are vendored; see `third_party/minhook/LICENSE.txt` and `third_party/minhook/UPSTREAM.md`. No game SDK headers or UE4SS components are linked.

`src/build_profile.h` is generated from the local executable copy by `tools/make_native_profile.py`; the normal build uses the checked-in profile and does not require that executable. `tools/verify_native_build.py` audits the built DLL and the analysis profile. `build/guard_probe.exe <absolute path to ASI>` validates rejection in an unsupported host process.

Static investigation is retained in the local workspace; game extracts and binary-analysis inputs are excluded from this repository. This implementation and analysis tooling were generated with AI assistance and have not yet been validated in-game. No release version or tag has been created.
