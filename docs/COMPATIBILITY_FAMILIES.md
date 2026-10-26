# Startup compatibility families

The release recognizes three audited compiler/layout families using one shared gameplay implementation. Resolution runs once on the startup worker; callbacks use cached results without runtime scanning or version branches.

| Executable | SHA256 | Family |
|---|---|---|
| Steam v2.0.6 | `61bc1e030740cebc30cf1dad0c86cf65e39e12ff0500225821d684181e08d56b` | inline |
| Steam v2.1.0 | `2bd3f115b959a8eee7e5c4b42b0142dd0025bd437e1014f92dfc60b9afee5479` | Steam 2.1.0 |
| Supplied alternative layout | `2ecc5d19fe37f97e3f7f2467d652b299b5a47f010fa49fd803a49a4a6930a409` | outlined |

Hashes identify the offline test inputs; there is no runtime whole-EXE hash allowlist. Each family must validate complete normalized function fingerprints, native references, globals and relevant layout witnesses. Changed, missing or ambiguous code refuses hooks. Address relocation can be tolerated; arbitrary future code changes require another reviewed profile.

All three inputs passed offline inspection and the corresponding code-mutation, ambiguity, relocation, native-reference and global-agreement tests. No game executable is included in the repository or releases. These tests read game images as non-executable data. Existing prior-release gameplay evidence on v2.0.6 remains historical; startup logs from the v2.1.0 candidates confirmed successful activation, but complete gameplay/save regression testing of these releases is still pending. The alternative input was not launched during this work.

Hold Fire validates 19 function/layout fingerprints, including its outlined aim helper where applicable, CapsuleComponent registration and shot/tick call relationships. Send to Stash validates 14 and selects an explicit per-family container-pool offset: 0x4db070 for Steam 2.0.6/2.1.0, 0x4d9070 for the alternative family. Callbacks and their behavior are shared.

The checked-in `address_profile_210.h` is regenerated with `make_210_profile.py <audited-v2.1.0-exe>` in scripts/ (Hold Fire) or tools/ (Send to Stash). Development dependencies are Capstone 5.0.6 and pefile 2024.8.26. The generator enforces the reviewed input hash and uses `src/profile_210.json`; no game file is needed to build an ASI from the checked-in headers.

See `compatibility-results.json` for sanitized local regression results. CI has no proprietary game inputs; it builds the release and runs binary-format, unsupported-host and packaging checks. Local offline results are separate from CI and gameplay evidence.
