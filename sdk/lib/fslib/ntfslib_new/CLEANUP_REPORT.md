# ntfslib_new — Cleanup Session Report (2026-07-22)

End-to-end record of the `/simplify` cleanup pass over
`sdk/lib/fslib/ntfslib_new/` on branch `dev/ntfs-bringup2-cflan`.
Deferred work extracted from this session lives in `ROADMAP.md`.

## Scope and process

- **Target**: the whole new library (46 files, ~6000 lines, all new vs
  `origin/master`), plus the minimal consumer adjustments it forced in
  `drivers/filesystems/ntfs_new/` and
  `modules/rosapps/applications/rosinternals/ntfsinfo/`.
- **Method**: four independent review agents ran in parallel, one per angle —
  **reuse** (re-implementations of existing ReactOS/SDK facilities, with
  km+um linkability verified against `ntoskrnl.spec`/`ntdll.spec`),
  **simplification** (dead code, copy-paste, redundant state),
  **efficiency** (repeated I/O, allocation churn, O(n²) scans),
  **altitude** (right-layer placement, public/internal/env boundaries) —
  cross-checked against an independent full read of all 46 files. Findings
  were deduped and applied; behavior-changing and correctness fixes were
  deliberately excluded (recorded in `ROADMAP.md` §1 instead).
- **Extra mandate**: resolve the actionable TODO/HACK comments in the code
  itself; feature-work TODOs were catalogued into `ROADMAP.md` instead of
  implemented.

## Result

- **Diff**: 40 files, **+483 / −930** (net **−447 lines**) while *adding*
  caching and chunking functionality. One file deleted
  (`src/volume/sanity.cpp`), zero files added (this report + roadmap aside).
- **Build**: `libntfslib_new.a`, `libntfslib_new_km.a`, `libntfslib_new_um.a`
  and `drivers/filesystems/ntfs_new/ntfs.sys` all compile and link
  (MinGW GCC 13 amd64). `ntfsinfo` is not part of this build configuration
  (`ENABLE_ROSAPPS=OFF`) — the previous "known-good baseline" had in fact
  never built it — so `ntfsinfo.c` was verified by standalone compile with
  the SDK include set; it compiles with only pre-existing warnings.
- Remaining build warnings are pre-existing, including `-Wparentheses` now
  flagging the `ClientMajorVersion = 0` typo (ROADMAP §1).

## Comment-driven actions resolved

| Original comment | Disposition |
|---|---|
| `ntfsattribdef.h` "TODO: Simplify … defining ReparsePointEx twice" | `ThirdPartyReparsePointEx` now embeds `ReparsePointEx Header;` (layout-identical, GUID stays at offset 8; both types had zero users) |
| `internal.h` "TODO: Make RootNode private when we abandon oldbtreefuncs" | oldbtreefuncs no longer exist anywhere; `RootNode` moved to `protected` beside `CurrentKey`; unused `PBTree` typedef dropped |
| `internal.h` "TODO: Maybe remove debug.h if NTFS_DEBUG isn't defined?" | Answered no: ~20 lib TUs get `DPRINT1` only through it and `NTFS_DEBUG` is never defined for the lib target. Include moved to the top with that rationale; the 214-line `#if 0` debug graveyard deleted; `PrintNTFSBootSector` moved beside its only caller (`volume.cpp`, still under `#ifdef NTFS_DEBUG`) |
| `internal.h` "HACK: tags should probably only be in *km target" | Wrong direction — the core passes tags into the alloc contract and each env decides whether to honor them. Comment rewritten; `#ifndef` guards dropped |
| `internal.h`/`km.cpp` "Hack: gShowMetadataFiles is a driver-specific setting" | Flag moved into the library (`NtfsShowMetadataFiles`, default FALSE) with public `NtfsSetShowMetadataFiles()`; driver `registry.c` calls the setter; the extern-into-consumer link dependency is gone |
| `ntfs_km.h` "HACK: temporary as I refactor this out of the public API" (×2) | `NtfsVolumeGetADSPreference` retyped `PFILE_OBJECT` → `PUNICODE_STRING` (it only ever read `FileName`) and declared in the public header — `PFILE_OBJECT` no longer appears in core; `NtfsDirectoryGetFileBothDirInfo` stays in `ntfs_km.h` with an honest comment (km-typed `FILE_BOTH_DIR_INFORMATION`) |
| `ntfs_km.h` "Hack: for other hacks" opaque typedefs | Deleted — duplicated `ntfslib_new.h` included ten lines above |
| `ntfs_um.h` "HACK: Don't expose this" + `ntfsinfo.c` "TODO: Maybe this should go in NtfsProbePartition?" | Root-caused together: `NtfsProbePartitionAndOpenVolume()` now reads sector 0 itself via the env contract; `BootSectorData` parameter dropped; the raw `NtfsReadVolume` export removed from `ntfs_um.h`; both call sites (driver `vol.c`, `ntfsinfo.c`) each lost ~25 lines of boot-sector plumbing |
| `volume.cpp` "TODO: Why doesn't this check work?" (Reserved3) | Root-caused: loop copied from `Reserved0` read 7 bytes of the 4-byte field, walking into the nonzero `Unknown` field. Re-enabled with bound 4 and an explanatory comment |
| `directory.cpp` "TODO: Should this be replaced with DiskVolume->UpcaseWideString()?" | Answered in a comment (volume `$UpCase` vs Rtl NLS trade-off; full switch needs the um name-matching fallback, ROADMAP §4); the per-key re-upcasing hoisted out (below) |
| `ntfslib_new.h` "Hack: we shouldn't be defining UINT" | Deleted; the five `UINT` uses became `ULONG` |
| `ntfslib_new.h` "TODO: Add Pimpl classes …" | Deleted — verified no consumer reaches class internals; the C API already implements the opaque-handle pattern |
| `um.cpp` "Hack: we shouldn't be defining these status codes" | Real codes via `ndk/umtypes.h` (the hand-rolled `STATUS_SUCCESS` was **1**); note: ReactOS `psdk/ntstatus.h` is itself disabled by `WIN32_NO_STATUS` and `psdk/winternl.h` is a `#error` stub — NDK is the canonical route |
| `um.cpp` "Hack: We shouldn't be using these pools in UM at all" | 25-line copied `POOL_TYPE` enum replaced by `typedef int POOL_TYPE;` with a comment (um allocator ignores pool semantics; ABI-identical) |
| `ntfsinfo.c` "Hack: We shouldn't be defining this here" (NT_SUCCESS) | Gone via the same NDK include |

## Dead code removed

- `src/volume/sanity.cpp` — entire file. It was **not listed in
  `src/CMakeLists.txt`** (never compiled) and referenced nonexistent symbols
  (`WriteBlock`, lib-side `PartDeviceObj`); its two `Volume` method
  declarations and the driver's stale commented call removed too.
- 8 exported C API functions with zero consumers (`NtfsDirectoryCreateEx`,
  `NtfsFileRecordGetData`, `NtfsFileRecordFindNonResidentData`,
  `…FindNonResidentDataFromAttribute`, `…CopyDataFromAttribute`,
  `…UpdateResidentData`, `…CommitFixup`, `…ApplyFixup`) plus the 2-arg
  `Directory` constructor only the dead `CreateEx` used.
  `NtfsFileRecordCreate`/`Destroy` kept as the lifecycle pair.
- `Volume` driver-VCB residue members (`Flags`, `OpenHandleCount`,
  `StorageDevice`, `StreamFileObject`, `VolParamBlock`, `PubFileObject`,
  `PartDeviceObj`) — all unused; the driver keeps its own copies in its VCB.
- `MasterFileTable::MftZoneReservation`, the never-instantiated `RestartPage`
  class (member literally named `Foo`), and the three declared-never-defined
  `LogFileService` methods (`LogTransaction`, `CommitTransaction`,
  `WriteCheckpointRecord`) → replaced by a one-line roadmap comment.
- Dead macros: `GetVCN` (wrong `PIndexEntry` pointer arithmetic),
  `GetIndexEntryVCN`, `GetSQNFromFileRef` (extracted the wrong bits),
  `CanDemoteAttribute` (referenced nonexistent `MustPromoteAttribute` —
  uncompilable), `NewAttributeEndPtr`, `IncrementUpdateSequenceNumber`
  (only used inside `#if 0`), commented-out `ROUND_DOWN`, `IsBitSet`, and
  the duplicate `GetWStrLength`/`MAX_SHORTNAME_LENGTH` re-definitions in
  `get.cpp` and duplicate contract declarations in `km.cpp`.
- Structurally dead `#ifdef NTFS_KERNEL_MODE PAGED_CODE()` blocks (the macro
  was defined only in a header the core never includes) and the
  `NTFS_KERNEL_MODE` define itself.
- 214-line `#if 0` debug-function graveyard in the internal header
  ("Most of these broke with the driver migration").

## Efficiency fixes

- **$LogFile mount read: full file → 8KB.** `InitializeLFS()` allocated and
  read the entire `$LogFile` `$DATA` (typically 64MB, one PagedPool block,
  retained for the volume lifetime) to look at two 4KB restart pages. Also
  NULLs `LogFileData` after the version-failure `delete` (the destructor
  frees it too — was a latent double free).
- **$UpCase cached.** `UpcaseWideString()` re-fetched and re-read the full
  128KB `$UpCase` table from disk on every call; now lazily loaded once into
  `Volume::UpcaseTable` (freed in `~Volume`), with a bounds guard.
- **$MFT/$MFTMirr run lists cached.** Every `GetFileRecord()` re-looked-up
  the `$DATA` attribute and re-decoded the whole run list (one pool
  alloc/free per fragment) per 1KB record fetch. `MasterFileTable` now caches
  the attribute + decoded runs (safe while cluster reallocation is
  unimplemented), a new `FileRecord::CopyData(Attr, PrecomputedRuns, …)`
  overload consumes them, and the new `~MasterFileTable` frees runs and the
  cached records (previously leaked on volume teardown).
- **$I30 runs decoded once per directory load.** `CreateRootNode()` decodes
  the index-allocation run list once and threads it through the `CreateNode()`
  recursion (was: one full decode+free per index buffer read).
- **GetFreeClusters chunked.** Was: allocate the entire volume bitmap
  (32MB at 1TB/4K clusters) in one NonPagedPool block, count via a private
  nibble table — including stale padding bits past the last cluster. Now:
  64KB chunks + `RtlInitializeBitMap`/`RtlNumberOfClearBits` (exported by
  both ntoskrnl and ntdll), counting exactly `ClustersInVolume` bits.
- **Search filter upcased once per operation** instead of per key tested:
  hoisted from `DoesFileNameMatch()` to `FindNextFile()` and
  `GetFileBothDirInfo()` (idempotent in-place upcase, same caller contract).
- **km bounce-buffer length exact**: `ALIGN_UP_BY((Offset % BPS) + Length, BPS)`
  instead of aligning `Length` and then unconditionally adding a whole extra
  sector for any unaligned offset (both read and write paths).
- Micro: `wcslen` of a constant per loop iteration in
  `IsLegalShortNameCharacterW` (now `static const` + NUL-terminated scan),
  `wcslen(AttributeTypeName)` computed twice, `min(...)` recomputed twice per
  run in the write loop (hoisted into `Chunk`), pointless `if (Offset)`
  guards before `Offset = 0`.

## Reuse fixes

- `NtfsFillMemory` env wrapper (3 definitions for a macro-in-both-envs)
  deleted; the one caller (the `#if DBG` 0xCD fill in `util.cpp`) calls
  `RtlFillMemory` directly.
- `ROUND_UP`/`ULONG_ROUND_UP` replaced by SDK `ALIGN_UP_BY` (both uses are
  power-of-two); private popcount table replaced by RTL bitmap API (above).
- `km.cpp` now includes `ntfslib_new_internal.h` (env/km CMake gained the
  include dirs) instead of hand-duplicating the operator-new prototypes,
  tag, and extern declarations — signature drift is now a compile error
  instead of a silent ABI hazard.
- Verified non-findings recorded by the reuse agent: `WideStringCompare` is a
  legitimate adapter (plural `RtlCompareUnicodeStrings` is not km-linkable);
  no shared SDK header exists for the NTFS on-disk structs (this library's
  public header is the candidate to become it); `RtlIsNameLegalDOS8Dot3`
  deferred per the author's tested-and-documented comment (ROADMAP §3).

## API / layering fixes

- `NtfsProbePartitionAndOpenVolume(BytesPerSector, PNtfsVolume*)` — reads the
  boot sector itself; typed out-param instead of `void**`.
- `NtfsFileRecordCreate(PNtfsVolume, …)` instead of `void*`.
- LFS client-version getters return `ULONG` instead of `NTSTATUS`.
- In-memory `_BTreeNode`/`_BTreeKey`/`DataRun` structs moved from the public
  header to the internal one (zero external users; the C APIs returning
  `PDataRun` were dead and removed — which also closes the
  "returns memory the consumer cannot free" API gap).
- `INDEX_ENTRY_NODE`/`INDEX_ENTRY_END` moved next to their on-disk struct
  (`IndexEntry`) in `ntfsattribdef.h`.
- Magic `<= 26` metadata cutoff named: `NTFS_LAST_RESERVED_FILE_RECORD` next
  to the `FileRecordNumbers` enum.
- Include guards added to the three guard-less headers (internal, km, um);
  `ntfs_km.h` uses `<ntfslib_new.h>` instead of a relative path.
- `} PRestartArea;` value-typedef with a pointer name fixed to `*PRestartArea`.
- `GetSubnodeVCN` macro (the one correct VCN-extractor) parenthesized and now
  actually used at its one open-coded site; predicate macros
  (`IsLowercaseCharacterW`, `IsDotOrDotDotDirW`, `IsRootFile`,
  `MayHaveShortKey`, `IsEndOfNode`, `LONGLONG_SIGN_EXTEND`,
  `GetFRNFromFileRef`) parenthesized; triple-nested-ternary `GetNextKey`
  macro converted to a `static inline` function; `OffsetToFirstUSN` now uses
  its parameter (one call site was passing the *class name* `Volume` and
  compiling only because the argument was discarded).
- `FileRef()` macro used at its one bypassed site; `GetRunSize()` used at two
  open-coded `SectorsPerCluster * BytesPerSector` sites.
- addattr.cpp zero-fill/DataLength logic deduplicated across the grow/no-grow
  arms (equivalence argued: growth implies `newDataEnd > oldDataLen`);
  `FindNonResidentData` NULL-check moved before the initializer that
  dereferenced the argument (the check was dead as written).
- btree.cpp: destructor's redundant empty-root special case collapsed into
  `DestroyBTreeNode` (whose `NextKey` is now initialized — the `ASSERT` read
  it uninitialized for empty nodes); helpers made `static`, as was
  `WideStringCompare`; `metadata/volume.cpp` no-op `goto Done` before `Done:`
  removed; `GetADSPreference` duplicated copy-loop hoisted into
  `CopyStreamName()`.
- README: `ntfslib_new_fl` marked "(planned, not implemented yet)".

## Consumer-side changes (forced by the above)

- `drivers/filesystems/ntfs_new/vol.c` — boot-sector alloc/read/free plumbing
  removed around the probe call (−28 lines incl. the stale
  `RunSanityChecks()` comment).
- `create.c` / `write.c` — pass `&FileObject->FileName` to
  `NtfsVolumeGetADSPreference`.
- `registry.c` / `ntfspch.h` — `gShowMetadataFiles` global removed; registry
  value applied via `NtfsSetShowMetadataFiles()`.
- `ntfsinfo.c` — boot-sector read block removed; NDK include replaces the
  `NT_SUCCESS` hand-define.

## Explicitly skipped (with reasons) → tracked in ROADMAP.md

- All correctness bugs found during review (ROADMAP §1) — out of cleanup
  scope by policy.
- The `#include <ntifs.h>`-in-core architectural refactor and env-contract
  header (§2) — real refactor, not cleanup.
- `mft/get.cpp` tail dedup (§5) — the two tails differ semantically
  (magic-failure falls back to `$MFTMirr`, fixup-failure must not); merging
  risked silent flow change.
- 8.3/NameType redesign, lazy descent, per-FCB caching (§4) — algorithm
  changes in WIP code.
- `RtlIsNameLegalDOS8Dot3` swap (§3) — author documented it was tried and
  doesn't work; needs Windows-behavior tests.
- Member-capturing-macro → member-function conversion, LFS header split,
  UNICODE_STRING helper, allocator-pairing consistency (§5) — churn deferred.
- `dbg.cpp`, `editdir.cpp` stubs — intentional bringup tooling/roadmap.

## Verification detail

1. Incremental ninja builds of `libntfslib_new.a`, `libntfslib_new_km.a`,
   `libntfslib_new_um.a`, `drivers/filesystems/ntfs_new/ntfs.sys` — all link.
2. `ntfsinfo.c` standalone-compiled (rosapps disabled in this output dir).
3. Repo-wide grep sweep for every removed symbol/macro — no stale references.
4. During review, a leak was caught and fixed in the new code itself
   (`CreateRootNode` decoding runs before the corruption-check early return).
