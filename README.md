# Send to your stash

Native ASI test build for S.T.A.L.K.E.R. 2. Adds **Send to your stash** to the existing inventory right-click menu, from anywhere. No UE4SS dependency and no replacement game assets.

The implementation uses the personal-stash container identified from the premium-edition Journalist flash-drive reward, then calls the game's existing item-transfer/stack-selection path. It only offers the action for carried items whose native Drop permission allows it and whose Unequip action is not visible.

Beta `v0.1.0-beta.2` updates packaging to avoid shared documentation filenames in Vortex; the ASI code is unchanged. The author confirmed in-game operation on October 21, 2026, including save/reload persistence, partial-stack transfers, stack cancellation, and retention of weapon condition, attachments, and ammunition. Weapon upgrade retention has not been separately confirmed.

## Current test

Download one package and extract it beside `Stalker2-Win64-Shipping.exe` in `Stalker2/Binaries/Win64`:

- `SendToStash-ASI-only.zip`: use when an ASI loader is already installed.
- `SendToStash-ASI-with-loader.zip`: includes Ultimate ASI Loader 9.7.4 as `winhttp.dll`. If that DLL already exists, keep your existing loader and use the ASI-only package instead.

Both packages contain the same ASI plus instructions, checksums, and applicable third-party licenses. No UE4SS installation is required.

Documentation and internal checksum filenames start with `SendToStash` to avoid Vortex conflicts with other ASI mods. When using PlayerObstructionHoldFire alongside this mod, install the loader with only one mod and choose the ASI-only package for the other. Two loader-bundled packages still share `winhttp.dll`.

When upgrading in Vortex, replace the previous SendToStash installation and redeploy so its old generic documentation files are removed. For a manual upgrade, remove obsolete documentation from the old package only when it is not shared with another mod.

Supported executable SHA-256: `61bc1e030740cebc30cf1dad0c86cf65e39e12ff0500225821d684181e08d56b`. Different executables or mismatching native entry bytes disable the plugin before hooks are installed.

`build/SendToStash.asi` is the test artifact. `tools/install_test.ps1` checks the installed executable, requires an existing recognized ASI loader, and copies only this plugin. It backs up an earlier copy and does not replace proxy DLLs. The plugin writes `SendToStash.log` next to itself. Restart the game after installation or removal; do not unload it during play.

To uninstall this test, close the game and remove only `SendToStash.asi` from `Stalker2/Binaries/Win64` (or restore the prior-copy backup listed in `build/installation.json`). Leave the existing loader and other plugins alone.

For regression testing on future game builds, use a manual save: right-click one ordinary carried item, send it, and confirm it disappears from inventory and appears in a base's personal stash. Check partial-stack transfers and cancellation, another stash location, save/reload, and weapon condition, upgrades, attachments, and loaded ammunition.

## Build and evidence

GitHub Actions builds `SendToStash.asi` on Windows using the same script, Zig 0.15.2, vendored MinHook, and checked-in game profile. Each push, pull request, or manual run produces a `SendToStash-windows-x64` artifact containing both ZIP variants and their SHA-256 checksums. Download it from the run's Artifacts section. `tools/package.py` downloads the official loader, verifies its pinned archive and DLL checksums, and verifies both ZIPs.

Pushing a version tag such as `v0.1.0-beta.2` also publishes a GitHub prerelease after all build checks pass. Release assets include both ZIP variants and their checksum file. To publish manually, run this workflow with an existing `v*` tag selected. Existing releases are not overwritten. Branch builds only upload Actions artifacts. Beta builds remain marked as prereleases.

CI checks the DLL exports/imports and runs the unsupported-host guard. It does not require or distribute the game executable, and does not test gameplay. Debug information includes build paths, so the CI ASI is not guaranteed byte-identical to a local build. The menu label is currently English-only, without a localization SID.

Run `tools/build_native.ps1 -Zig <zig.exe>` with Zig 0.15.2. The MinHook C sources are vendored; see `third_party/minhook/LICENSE.txt` and `third_party/minhook/UPSTREAM.md`. No game SDK headers or UE4SS components are linked.

`src/build_profile.h` is generated from the local executable copy by `tools/make_native_profile.py`; the normal build uses the checked-in profile and does not require that executable. `tools/verify_native_build.py` audits the built DLL and the analysis profile. `build/guard_probe.exe <absolute path to ASI>` validates rejection in an unsupported host process.

Static investigation is retained in the local workspace; game extracts and binary-analysis inputs are excluded from this repository. This implementation and analysis tooling were generated with AI assistance. In-game operation has been confirmed by the author; automated CI checks do not run the game.
