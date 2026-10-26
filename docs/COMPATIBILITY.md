# Startup-only compatibility development

**Current release:** v0.2.0; see [three-family compatibility](COMPATIBILITY_FAMILIES.md). The older single-family implementation and its gameplay evidence below are historical.

Beta v0.1.0-beta.3 uses the **unified ASI**, which always runs startup address resolution and uses one callback implementation. No UE4SS runtime dependency is added to the ASI. The author confirmed the unified ASI works in-game on Steam v2.0.6 on October 22, 2026.

## In-game regression confirmation

On October 22, 2026, the author tested the previous two-path standalone ASI on the base **Steam v2.0.6** game executable and confirmed partial transfers, cancellation, and preservation of weapon ammunition, attachments, and upgrades. Save/reload persistence and weapon condition retention were confirmed in earlier testing on October 21. That test validated the previous known-build gameplay path; it did not exercise the unified implementation or establish support for the development-only fixture.

## Cost and behavior

- There is no known-executable hash shortcut in this test build. Testing on the installed Steam build therefore exercises the actual resolver and resolved-address callbacks.
- `address_resolver.c` makes one bounded startup pass over the PE unwind-function table. It searches for complete instruction fingerprints, masking address-dependent PC-relative operands while preserving instruction selection, member offsets, enum values, and internal branches.
- Fourteen function/layout profiles, 17 native call relationships, and agreement among six manager-global references are required. Compiler-identical small getters are disambiguated using those native references; ambiguous or missing matches disable the hooks.
- All validated executables use the callbacks from `stash_runtime.inc`, compiled once in `send_to_stash.c`. The addresses are stored once after startup resolution.
- There are no signature scans, polling, or compatibility checks in the callbacks. Unlike the previous constant-address path, callbacks now read resolved-address table entries. The previous identical-assembly claim no longer applies; no measured performance comparison is available.
- The resolver supports address movement while the validated instructions and layout evidence remain unchanged. It intentionally does not adapt to changed object layouts or accept a similar-looking function with different unmasked instructions.

Matching these spans is a conservative compatibility heuristic, not a proof that every transitive callee, object initializer, or game behavior is unchanged. New game versions still need in-game regression testing before being advertised as supported. No performance or compatibility claim is made for an untested future executable.

## Development-only fixture

SHA-256: `2ecc5d19fe37f97e3f7f2467d652b299b5a47f010fa49fd803a49a4a6930a409`

Size: 174,730,288 bytes.

Role: **offline development fixture only; never launch it, install it, use it for gameplay, or add it to the supported-build hash list**. The local path and analysis inventory are recorded in ignored `research/dev-fixtures.json`; no executable is committed or packaged.

All seven old function entry locations differ in this fixture, and its data sections have moved. The fixed-address validator correctly rejects it. The relocated resolver also rejects it because some relevant instructions and layout constants changed. This is evidence that removing the original hash guard alone would be unsafe; it is not evidence that this fixture is playable with the mod.

For example, the baseline selection handler at RVA `0x34f779e` has a close counterpart at fixture RVA `0x3455a60`, but at function offset `0x2f3` its `add rax, 0x453060` is now `add rax, 0x451060`. The quantity helper has the same changed pool offset (baseline RVA `0x35ddfd8`, fixture RVA `0x35da0c6`). The stash-layout witness also has changed instructions and size. These are deliberately not wildcarded. Detailed candidate evidence is retained locally in ignored `research/relocation-fixture-candidates.json`.

## Local verification

The relocation probe reads PE files into PAGE_READWRITE memory without loading a game module or executing game code. `tools/test_address_resolver.ps1` additionally tests a moved transfer function with repaired relative calls, a moved manager global, changed unmasked code, duplicate matches, inconsistent call targets, and conflicting manager references. All pass, including rejection of the development-only fixture. The relocation tests use synthetic memory changes; they do not modify files or run game instructions. Baseline resolution took about 0.5 seconds on this machine, on the startup worker. Every executable now takes this same resolution path.

`build/relocation-fast-path.json` records the historical comparison for the previous two-path implementation. `build/address-resolver-tests.json` records the repeatable offline resolver tests. Generate the relocation profile with `tools/make_address_profile.py <audited-executable-copy>`; normal builds use the checked-in generated header and require no game executable.

## Test and rollback

The new artifact is `build/SendToStash.asi`. With the game closed, copy it over the installed ASI and keep the existing loader. On Steam v2.0.6, the log must contain `SendToStash unified ASI test: startup signature resolution required`, the 14 validated fingerprints, and `ready: ... resolved-address callbacks; 2 hooks enabled; no runtime scanning`.

Test single-item and partial transfers, cancellation, weapon condition/upgrades/attachments/ammo, and save/reload. A one-time startup scan is expected; no scan should repeat when opening the inventory or selecting an action.

If anything regresses, close the game and restore `build/rollback-pre-unified/SendToStash.asi` to the game's Win64 folder. Its SHA-256 is `ae8c425b77e84239ffbc86867d8dd1cc8be4c098c6dbc1897eae489ef7c294b4`. The same snapshot directory contains the previous source, build script, README, and this document for a source rollback. The rollback artifact is local only and is not included in release packages.
