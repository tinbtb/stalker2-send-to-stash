# Startup compatibility families 

Both native ASIs now recognize two audited compiler/layout families. There is no
storefront check or executable-hash allowlist at runtime. Each family must match
in full: normalized functions, native relationships, globals, and outlined helper
code. Matching individual functions from different families is not sufficient.

The installed Steam v2.0.6 executable (SHA256 beginning `61bc1e03`) selects the
inline family. The development-only fixture (SHA256 beginning `2ecc5d19`)
selects the outlined family. The fixture is read as data only, never launched,
installed, modified, or used for gameplay by the development tools. No executable
or machine-specific paths are included in the source or packages.

## What differs

SendToStash: the item pool changes from `0x453060` to `0x451060`, and the
container pool from `0x4db070` to `0x4d9070`. Three full transfer/selection
fingerprints preserve the correct item-pool constant. The outlined quest witness
calls a 74-byte stash-container helper; its complete fingerprint preserves
`manager+0x80`, the container pool, 2048-entry blocks, 0x1d0 stride, and 0x10
object offset. Both its caller and its call to the resolved native store getter
must agree. The one callback implementation uses the selected container pool.

Hold Fire: 18 of 19 function fingerprints retain their normalized instruction
sequences. The aim routine changes register allocation and outlines its fallback
into a 47-byte helper. That helper loads `actor+0x1c0`, uses the transform at
`+0x1d0` or the default transform, and copies twelve qwords to the output. Its
complete fingerprint, caller reference, and unwind extent are required. The
native policy, geometry, per-shot pointer checks, and hook sites are unchanged.

## Coverage and limits

An executable from another storefront or with unrelated edits may work when it
matches either complete family. Function and global addresses may move. Neither
family requires a whole-file hash at runtime. Missing, altered, or ambiguous
relevant code refuses hooks and clears the resolved output.

This does not recognize every possible compiler translation of equivalent game
logic. New instruction sequences or layouts require another reviewed family;
arbitrary similarity would not establish compatible ABI, ownership, or save
behavior. Offline matching is not gameplay validation.

Resolution runs only on the startup worker. The second family is tried only
when the first does not validate. There are no scans, hashes, or family-selection
branches in the firing or inventory callbacks. SendToStash reads one cached pool
offset when validating the stash container. No UE4SS dependency is introduced.

## Validation and next test

Both input executables pass offline resolution, unrelated identity changes,
function/global relocation, altered-code rejection, duplicate/ambiguous entries,
reference disagreement, helper mutation, and helper-call mutation checks.
SendToStash also rejects mixing the other family's item-pool constant into a
transfer routine. Hold Fire tests both unwind-function and leaf relocation,
CapsuleComponent-name changes, and malformed headers. Refusal clears outputs.

The author confirmed both new ASIs work in-game on the installed Steam v2.0.6
game on October 22, 2026. Earlier detailed gameplay checks covered inventory
transfer, partial stacks, cancellation, weapon state, and save/reload for
SendToStash; friendly hold-fire, hostile firing, and friendly-to-hostile
transitions for Hold Fire. The development-only executable remains offline-only. Gameplay support for
an additional release requires a separate permitted test installation.

Release versions: SendToStash v0.1.0-beta.4 and Hold Fire v0.5.0-beta.2.
