# ntfslib_new — Roadmap

Deferred work collected from the 2026-07-22 four-angle cleanup review
(reuse / simplification / efficiency / altitude) plus every actionable
TODO/HACK still in the code. See `CLEANUP_REPORT.md` for what was already
done. Paths are relative to `sdk/lib/fslib/ntfslib_new/` unless noted.

**2026-07-23 implementation pass:** all bounded correctness findings from
section 1 are resolved, along with the checked architecture, functionality,
performance, and cleanup items below. The unchecked entries are larger feature
projects or ABI/format changes—principally portable disk contexts, cluster
extent management, EFS, and LFS recovery/journaling—and
remain explicit rather than being represented by unsafe partial stubs.

**2026-07-23 Linux bring-up:** a native `NTFSLIB_PORTABLE` type/environment
layer, positioned `pread` disk glue, neutral `NtfsDirectoryEntry` enumeration
API, and FUSE 3 frontend now live under `env/linux` and `linux/`. The frontend
supports `getattr`, `readlink`, `access`, `readdir`, `open`, `read`, `release`,
and `statfs`, plus direct image inspection commands. Explicit mutation probes
use positioned `pwrite` on disposable images; FUSE mounting and ordinary
inspection stay read-only. The remaining architecture item below stays open
because ReactOS builds still use `ntifs.h` and all env implementations still
hold process-global disk state.

**2026-07-24 core-first priority:** FUSE is only a test surface. Development
order is now (1) on-disk correctness and complete read semantics, including
reparse points, security descriptors, EFS and WOF streams, and corruption
handling; (2) allocation, attribute growth, and directory mutation; (3) LFS
logging, replay, and crash recovery before writable mounts; (4) portable
per-volume I/O contexts and concurrency; then (5) frontend features.
Performance changes belong in the shared core and must preserve byte/entry
equivalence with an independent NTFS implementation.

**2026-07-24 write groundwork:** resident data can grow, shrink, and compact
inside a validated file-record buffer. Existing ordinary `$DATA` streams can
also promote from resident to nonresident, allocate clusters through
`$Bitmap`, extend mapping pairs, zero uninitialized gaps, and synchronize
duplicated sizes in local and parent-index `$FILE_NAME` values. The same paths
cover named streams. Existing base-record sparse streams additionally allocate
only holes intersected by a write, preserve untouched virtual ranges, resize
EOF, materialize on sparse clearing, punch full compression units for zero
ranges, and report allocated ranges. ReactOS exposes the shared sparse
primitives through `FSCTL_SET_SPARSE`, `FSCTL_SET_ZERO_DATA`, and
`FSCTL_QUERY_ALLOCATED_RANGES`. Fresh-image promotion, ordinary extension, and
independent NTFS-3G sparse fixtures matched `ntfscat`, consumed or released
the exact expected cluster counts, exposed the expected `$I30` sizes, and
passed `ntfsfix -n`. These are bounded correctness primitives, not
authorization for a writable mount: the uncommon no-movable-attribute initial
list case, unnamed file/directory creation, and LFS transactions remain open
below.

**2026-07-24 creation and `$I30` B+tree mutation:** ordinary base MFT file
records now allocate, initialize, publish, and roll back. A new file receives
V1 `$STANDARD_INFORMATION`, an indexed POSIX `$FILE_NAME`, an empty resident
unnamed `$DATA`, and inherited compact resident legacy security; a new
directory receives an empty `$I30` `$INDEX_ROOT` (collation 1, volume index
sizes) and, matching Windows, no default ARCHIVE bit. The public counted
UTF-16 entry point is `NtfsMasterFileTableCreateFile`; the Linux frontend
drives it through `--create-file`/`--create-dir`. Directory insertion covers
every tree shape: growth inside the resident root, first-overflow push-down
of the complete root list into index buffer 0 (creating `$BITMAP` and a
promoted nonresident `$INDEX_ALLOCATION` inside one record commit after the
root shrinks in memory), in-place leaf insertion, median leaf splits with
right-node-first publication, recursive parent insertion along the recorded
descent path, and root push-down at any depth. Two latent write-path bugs
fell out: `MasterFileTable::WriteFileRecordToMFT` now snapshots the
fixup-committed image and immediately restores the live record, because
writing `$MFT` record 0 through its own scrambled buffer corrupted the
attribute walk once mapping-pair growth pushed `$BITMAP` bytes across a
sector tail (main/`$MFTMirr` divergence at 1,065 same-directory creations);
and `PromoteResidentData` accepts `TypeIndexAllocation`. Verified on fresh
images: 1,295 single-directory creations through the shared core alone
returned the exact `ntfsls` name set, exact `ntfscat` bytes after writes,
exact lookups at collation extremes plus case-insensitive forms, contiguous
MFT record allocation with literal `$MFT:$BITMAP` bit checks, rollback
continuity across dozens of forced failures, and `ntfsfix -n` throughout;
saturation past the base record's mapping-pair room fails cleanly with
`STATUS_BUFFER_TOO_SMALL` until initial `$ATTRIBUTE_LIST` creation lands.
The `ntfslib_linux_create` test pins the full matrix (duplicates through
`$UpCase` collation, illegal and dot names, trailing separators, missing and
non-directory parents, attribute flags, parent timestamp deltas, 300-entry
pure-core split growth, and a 413-entry NTFS-3G-built tree extended at every
position). Release, ASan/UBSan (leak detection on), all 14 registered tests,
and the four ReactOS targets pass. Directory entry removal, rename, hard
links, and crash atomicity remain open below.

**2026-07-24 namespace operations and directory scaling:** deletion,
rename/move, and hard links are implemented end to end. `$I30` removal
handles every tree shape: direct cuts in the root and in leaves, internal
entries replaced by the collation maximum pulled from their subtree (with
END-child rewiring when a right spine runs dry), released END-only chains,
and replacement overflow routed through the same split-and-promote path as
insertion. `NtfsMasterFileTableDeleteFile` enforces Win32
DeleteFile/RemoveDirectory semantics (directory emptiness through real
enumeration), drops one name and its DOS/WIN32 alias per call, maintains
the hard-link count, and on the last name captures every VCN-zero
composite run across the `$ATTRIBUTE_LIST`, blanks extension records and
the base record, and only then returns the clusters to `$Bitmap`.
`NtfsMasterFileTableRenameFile` covers same-directory, cross-directory,
and case-only renames with collision checks that exempt only the renamed
file itself, publishes the new name before retiring the old one, and
shares one parent record object when source and destination directories
coincide. `NtfsMasterFileTableCreateHardLink` appends an additional
`$FILE_NAME` (unnamed-attribute duplication is now permitted for
`$FILE_NAME` only) and rolls the record back if the index insertion fails.
Two scaling changes lift the per-directory creation bound: the nonresident
extension path creates the initial `$ATTRIBUTE_LIST` proactively while the
base record still has headroom (ordinary records only), after which the
existing staged relocation spills crowded `$INDEX_ALLOCATION` segments
into extension records; and `$MFT` data growth reserves 16-cluster chunks
so its record 0 no longer floods with one mapping pair per interleaved
cluster. A self-entry in the created list is skipped during deletion's
run capture so the list cluster is not double-freed against `$Bitmap`'s
transactional double-free detection. Verified: 5,000 creations in one
directory with zero failures and the exact `ntfsls` name set; interleaved
deletion of every third entry and a full drain returning identical name
sets at each stage; empty-directory removal; free clusters returning to
baseline minus only the permanent chunked `$MFT` growth; MFT record reuse
from the lowest freed ordinal; link counts observed by `ntfsinfo` through
link creation and removal; byte-identical `ntfscat` reads through renamed
and linked paths; and `ntfsfix -n` after every phase. The
`ntfslib_linux_namespace` test pins the matrix, including a
1,500-entry listed-directory grow/drain/reclaim cycle.

---

## 1. Correctness bugs (known, deliberately NOT fixed during cleanup)

Found while reviewing; left untouched because the cleanup pass was scoped to
not change behavior. These should be the very next thing addressed
(`/code-review` territory):

- [x] `include/internal/ntfslib_new_internal.h:925` — `else if (ClientMajorVersion = 0)`
      assignment instead of `==` in `IsSupportedClientVersion()`; clobbers the
      member and the branch is always false. (Now visible as a `-Wparentheses`
      build warning.)
- [x] `src/mft/get.cpp:190,201` — `GetFileAttributeFromFileRecordNumber()` tests
      `!TargetAttribute` (the out-parameter pointer, never NULL) instead of
      `!*TargetAttribute`. The `$MFTMirr` fallback is dead and a missing
      attribute is returned as `STATUS_SUCCESS` with `*TargetAttribute == NULL`
      (callers then `GetResidentDataPointer(NULL)`).
- [x] `$MFTMirr` bootstrap — the old constructor read the first bytes at the
      mirror LCN as though they were MFT record 1. Those bytes are actually a
      copy of MFT record 0, so mirror writes silently targeted `$MFT` twice.
      Record 1 is now loaded from the main `$MFT` stream after bootstrapping
      record 0. Fresh-image writes reached the distinct main/mirror offsets
      and passed `ntfsfix -n`.
- [x] `src/mft/get.cpp:265-291` — `GetFileRecordFromQuery()` double-deletes
      `CurrentDirectory` on the `GetFileRecord`-failure path (deleted at :270,
      deleted again at :277/:291 without being reallocated).
- [x] `src/filerecord/write/filedata.cpp:148` — multi-run non-resident writes
      write from `Buffer` instead of `Buffer + BytesWritten` for every run after
      the first (first-run data is written repeatedly).
- [x] `src/volume/metadata/attrdef.cpp:134,150` — `GetADSPreference()`
      `filename::$TYPE` path assigns the local parameter (`RequestedStream = NULL`)
      instead of `*RequestedStream = NULL`, so the caller's variable is
      uninitialized; the error path at :150 can then `delete *RequestedStream`
      on that uninitialized value. The parser now also honors the counted
      `UNICODE_STRING` boundary, never mutates the caller's type suffix, and
      rejects extra separators or ADS syntax on a non-final path component.
- [x] `env/um/um.cpp:98` — um `NtfsReadVolume()` ignores its `Offset` parameter
      entirely (sequential `ReadFile` from current position; self-labeled hack).
      Everything beyond reading sector 0 is broken in usermode. Fix with
      positioned I/O — `NtReadFile` with explicit `ByteOffset` (prior art:
      `sdk/lib/fslib/vfatlib/common.c:85`) or `ReadFile`+`OVERLAPPED`.
      `NtfsWriteVolume()` now uses the same positioned-I/O contract.
- [x] `src/filerecord/read/copy.cpp:122` — early return on `ReadVolume` failure
      leaks the locally-decoded data-run list (when `PrecomputedRuns == NULL`).
- [x] `src/volume/volume.cpp` — `Volume::Initialize()` never frees the
      `VolumeFile` record it fetches for `$Volume` version info.
- [x] `src/capi/volume.cpp:37` / public header — `NtfsVolumeGetBytesPerSector()`
      returns `UINT8`, truncating 4096-byte sectors to 0. Should be `ULONG`
      (small ABI change; one driver consumer).
- [x] `src/btree/directory/find.cpp:19` — `CompareLength` yields a CHARACTER
      count but `WideStringCompare()` consumes its `Length` as BYTES
      (`UNICODE_STRING.Length`), so collation-based subtree skipping compares
      only half the intended length. Can cause wrong descend/skip decisions.
- [x] `src/mft/get.cpp:224` — `INVESTIGATE ME` marker: `GetFileRecordFromQuery()`
      called with NULL query (find the caller and decide the contract).

## 2. Architecture — road to a truly portable core

The one big hack everything else hangs off:

- [ ] `include/internal/ntfslib_new_internal.h:4` — `#include <ntifs.h>` in the
      shared core ("Hack: Bad! Km only!"). The core is compiled once against
      kernel headers and linked into a Win32 app. Kill it in steps:
      1. Create a self-contained `include/internal/ntfsenv.h` env-contract
         header (no ntifs.h) declaring `NtfsAllocatePoolWithTag`, `NtfsFreePool`,
         `NtfsReadVolume`, `NtfsWriteVolume`, `NtfsIsNameInExpression`, and the
         pool `operator new` overloads; include it from core + all env glue
         (removes um.cpp's local `POOL_TYPE`/decl duplication for good).
      2. Replace `POOL_TYPE` in the contract with an env-neutral alloc-hint
         enum (`NtfsPagedAlloc` / `NtfsNonPagedAlloc`); km maps it to real pools.
      3. Replace the process-global env I/O state (`PartDeviceObj` /
         `VolumeHandle` singletons in `env/km/km.cpp:26`, `env/um/um.cpp`) with
         an opaque disk-context handle returned by `NtfsDiskInitialize*` and
         stored in `Volume` — the library currently supports exactly one
         mounted volume per process while pretending otherwise.
      4. Then swap `<ntifs.h>` for `<ntdef.h>`/NDK types in the internal header.
- [ ] `env/fl/` — FreeLdr glue is an empty stub (CMake commented out, README
      marks it planned). Becomes implementable only after the steps above.
- [ ] Pageability: `NTFS_KERNEL_MODE`/`PAGED_CODE()` conditionals were removed
      as structurally dead (a compile-once core can't see a km-only define).
      If pageability assertions are wanted, add `NtfsAssertPageable()` to the
      env contract (km: `PAGED_CODE()`, um/fl: no-op).
- [ ] `env/km/ntfs_km.h` — `NtfsDirectoryGetFileBothDirInfo` could move to the
      public header once um consumers can see `FILE_BOTH_DIR_INFORMATION`
      (`sdk/include/ndk/iotypes.h:738`), or — deeper cut — return a lib-neutral
      directory-entry struct and let the driver format `FILE_BOTH_DIR_INFORMATION`.
      The neutral `NtfsDirectoryEntry`/`NtfsDirectoryReadNext` half now exists
      and is used by FUSE; migrating the ReactOS driver formatter remains.
- [x] Per-volume options: `NtfsShowMetadataFiles` is now lib-owned but still a
      process global; make it a `Volume` field (or a flags argument to
      `GetFileBothDirInfo`) set after mount.

## 3. Missing NTFS functionality (TODOs living in the code)

### Name matching
- [x] `src/NtfsIsNameInExpression.cpp:15` — the portable fallback is a stub
      returning FALSE, and it is the LIVE path for wildcard matching on
      ReactOS usermode: ReactOS ntdll does **not** export
      `RtlIsNameInExpression` (verified absent from `dll/ntdll/def/ntdll.spec`),
      so the `GetProcAddress` path in `env/um/um.cpp` only works on real
      Windows 7+. Don't hand-write a matcher — either share
      `FsRtlIsNameInExpressionPrivate` (`ntoskrnl/fsrtl/name.c:21`) as a static
      lib, or add the `RtlIsNameInExpression` export to ReactOS ntdll. Resolved
      with a bounded-memory matcher covering NT and DOS wildcard tokens.
- [x] `src/btree/directory/util.cpp:32` — decide `RtlIsNameLegalDOS8Dot3()` vs
      the custom `IsLegal8Dot3ShortName()`. The Rtl version was tried and
      "does not currently work for this purpose"; it is exported by both
      ntoskrnl and ntdll, so it IS linkable from shared core. Needs tests on
      real Windows before switching (note: the custom version accepts
      lowercase, which is itself questionable — on-disk DOS names are upcased).

### Attributes / file records
- [x] `src/filerecord/attributes/findattr.cpp:92` — search `$ATTRIBUTE_LIST`
      (0x20) when an attribute isn't in the base record (required for
      fragmented/large files). Extension records are sequence/base-reference
      validated and cached for the lifetime of the base record.
- [x] Merge the run lists of nonresident attribute segments referenced by
      `$ATTRIBUTE_LIST`. The loader validates ordered, contiguous VCN ranges,
      attribute IDs/names/flags, extension-record sequence/base references,
      and full logical stream coverage, then caches the composite run list.
      A fragmented 10,813,440-byte test stream split between MFT records 568
      and 570 matched `ntfscat` byte-for-byte with nonzero payload data.
- [x] Validate the nonresident `$ATTRIBUTE_LIST` bootstrap invariant. Its
      mapping pairs must fit in the base MFT record because locating a
      continuation would recursively require the unread list. The core
      requires unflagged, cluster-aligned allocation and complete base-run
      coverage rather than treating a split list as a supported layout.
- [x] Read and patch existing alternate data streams through the same core
      attribute APIs as unnamed data. Counted `file:stream` and
      `file:stream:$DATA` parsing is bounded and rejects malformed intermediate
      components. Fresh-image ADS fixtures now cover resident-to-nonresident
      promotion and nonresident allocation with a zero-filled gap; both
      matched `ntfscat` and passed `ntfsfix -n`.
- [x] Create missing named `$DATA` streams in an existing base file record.
      The shared writer inserts a bounded resident attribute with a unique
      instance ID, preserves NTFS's uppercase-primary/original-UTF-16-secondary
      name ordering, and immediately promotes large or offset writes through
      the normal allocation path. Resident and nonresident fresh-image streams
      matched `ntfscat`, including a zero-filled gap, and passed `ntfsfix -n`.
      Files that already have an `$ATTRIBUTE_LIST` can now allocate a free or
      newly extended MFT record, create the named stream there, update
      `$MFT::$BITMAP`, and insert the ordered list entry. A compact fixture
      created a nonresident extension-owned stream that matched `ntfscat` and
      passed `ntfsfix -n`; a focused exhaustion run also grew `$MFT` by one
      record and remained readable by NTFS-3G. The list's own nonresident
      allocation can grow while its mapping pairs continue to fit in the base
      record; an exhaustion run crossed 4 KiB, grew both the list and
      `$MFT::$DATA`, and passed `ntfsfix -n`. When a base record without a
      list fills, the core can now create the initial complete list, relocating
      a resident named stream, a legacy per-file security descriptor, or the
      inseparable resident `$EA_INFORMATION`/`$EA` pair into a validated
      extension record. The compact ADS and EA tests exercise those
      transitions before further mutation or NTFS-3G growth.
- [x] Enumerate every logical `$DATA` stream through a shared neutral API,
      including streams whose first extent lives in an `$ATTRIBUTE_LIST`
      extension record. The parser validates list bounds, base references,
      sequence numbers, attribute IDs/names, duplicate logical names, and
      resident/nonresident sizes. ReactOS now formats that result as
      8-byte-aligned `FileStreamInformation` entries and returns only complete
      entries on overflow. One compact fixture forced NTFS-3G to create 30
      streams across extension records and matched every name and logical
      size. The same shared paths now modify, resize, and promote resident
      named streams in extension records; already-nonresident extension
      streams support in-place writes, allocation growth/shrink, EOF changes,
      and independent preallocation while their mapping pairs fit the owner
      record. A resident named stream left in the base record after list
      creation can also promote in place: its existing list entry continues to
      identify the same record and attribute ID. The compact fixture promoted
      `alpha` from 19 to 12,282 bytes and matched `ntfscat` under Release and
      ASan/UBSan. File-wide last-write/change timestamps remain in the base
      record, and every form matched `ntfscat`.
- [x] Read ordinary NTFS-compressed `$DATA` streams. The shared core validates
      16-cluster compression-unit layouts, handles compressed, fully allocated,
      and fully sparse units, decodes LZNT1 with bounded chunk/back-reference
      checks, and supports unaligned/final-unit reads. A mixed 536,633-byte
      stream matched `ntfscat` byte-for-byte; one 1,000-open mountless sample
      took 3.73 s versus 7.19 s for `ntfscat`.
- [x] Read and validate `$REPARSE_POINT` values up to the NTFS/Windows 16 KiB
      limit, including both Microsoft and third-party common envelopes.
      Standard symbolic-link and mount-point payloads get bounded
      offset/length, flag, UTF-16, and tag validation through a zero-copy
      portable API. `ntfs.sys` now serves `FSCTL_GET_REPARSE_POINT`; the Linux
      frontend exposes native links through `readlink`. One 64 MiB image with
      an `IO_REPARSE_TAG_SYMLINK` produced `target.txt` through the direct core
      command, the ntfslib mount, and ntfs-3g, and ntfslib followed it to the
      same payload.
- [x] Create, replace, and delete native `$REPARSE_POINT` values. The shared
      core validates Microsoft/third-party envelopes, tag and GUID replacement
      rules, the 16 KiB ceiling, empty-directory and empty-symlink-stream
      constraints, and the native first-reparse-versus-EA conflict before
      mutation. Values stay resident when possible and otherwise use
      copy-on-write cluster allocation; old nonresident runs are released only
      after the new MFT record is durable. `$STANDARD_INFORMATION` plus every
      local and parent-index `$FILE_NAME` now track the reparse flag/tag,
      ARCHIVE, change time, and the restored EA-size union on deletion.
      `ntfs.sys` serves `FSCTL_SET_REPARSE_POINT` and
      `FSCTL_DELETE_REPARSE_POINT` with access, read-only-volume, ADS, and
      symlink-privilege checks and advertises reparse support. A fresh-image
      fixture covered resident symlink replacement, tag/GUID conflicts, an
      8,216-byte nonresident third-party value, exact cluster recovery,
      byte-for-byte `ntfscat`, `ntfsinfo`, and `ntfsfix -n`.
- [x] Return `STATUS_REPARSE` during ReactOS pathname creation so the object
      manager follows name-surrogate reparse points. A counted shared lookup
      stops on intermediate reparse records and on final records unless
      `FILE_OPEN_REPARSE_POINT` is present, returning the exact unparsed suffix.
      `ntfs.sys` copies the validated native buffer to nonpaged auxiliary
      storage, fills its `Reserved` suffix length, returns the tag through
      `IoStatus.Information`, and lets the I/O manager apply relative/absolute
      substitution and loop-depth policy. A focused image verified final-open,
      final-reparse, trailing-separator, and intermediate-suffix results under
      Release and ASan/UBSan; the ReactOS driver build passed.
- [x] Decode WOF/Compact OS FILE-provider data. The shared core validates the
      WOF envelope and cumulative chunk table, then transparently serves
      XPRESS4K, XPRESS8K, XPRESS16K, and LZX32K streams, including raw chunks
      and bounded unaligned reads. A test-only libwim fixture independently
      generated every format with mixed raw/compressed chunks; exact full and
      range reads, malformed-table rejection, Release, ASan/UBSan, and the
      ReactOS `ntfs.sys` build all passed.
- [ ] Decode WOF WIM-provider data, cloud placeholders, and other filter-owned
      payloads. Unknown tags are preserved by the raw API rather than guessed.
- [x] Materialize WOF-backed files and remove their external backing before
      writes. The shared core writes the complete decoded stream first, reuses
      an already complete ordinary allocation when safe, otherwise allocates
      replacement runs for sparse layouts, atomically switches the base record
      away from WOF metadata, then releases old backing and logical runs.
      Unnamed writes, truncation, and allocation changes invoke this path;
      ReactOS also invokes it on write-open and serves
      `FSCTL_DELETE_EXTERNAL_BACKING`. The focused fixture verified automatic
      and explicit transitions, exact data through `ntfscat`, exact cluster
      reclamation, absent reparse/backing attributes through `ntfsinfo`, and
      no mutation of corrupt backing. A zero-allocation sparse logical-stream
      fixture additionally materialized to a normal initialized stream and
      passed `ntfsfix -n`.
- [x] Read and validate native `$EA` plus its matching resident
      `$EA_INFORMATION`. The shared core checks record alignment and bounds,
      legal flags/names, terminators, packed/unpacked sizes, and `NEED_EA`
      counts, then exposes bounded zero-copy iteration. `ntfs.sys` serves
      `FileEaInformation` and `IRP_MJ_QUERY_EA`, including name-list queries,
      one-based indexes, restart/resume state, single-entry requests, missing
      name placeholders, and partial-buffer status. A two-entry fixture
      written by libntfs-3g produced the expected `AUTHOR=Ada` and
      `NEEDED=core` records through the shared API and passed an ASan run.
- [x] Read validated self-relative security descriptors from both legacy
      per-file `$SECURITY_DESCRIPTOR` attributes and NTFS 3.x centralized
      `$Secure:$SII` / `$Secure:$SDS` storage. The shared core validates SID,
      ACL, ACE, index-node, bitmap, SDS-header, length, offset, and hash
      invariants and retries the 256 KiB duplicate SDS copy when needed.
      `ntfs.sys` now serves `IRP_MJ_QUERY_SECURITY` through
      `SeQuerySecurityDescriptorInfo`, preserving requested-component and
      required-buffer semantics. Fresh-image direct and centralized fixtures
      matched `ntfscat` and `ntfssecaudit` byte-for-byte under Release and
      ASan/UBSan.
- [ ] Create or replace security descriptors and maintain `$SDS`, `$SII`,
      `$SDH`, reference counts, and file `SecurityId` values transactionally.
      `IRP_MJ_SET_SECURITY` remains intentionally unregistered until these
      shared-core primitives and LFS logging exist.
- [x] Create, replace, and remove native EAs through one shared batch API.
      Updates use case-insensitive, ordered last-wins semantics and are fully
      validated/merged before mutation. The core rebuilds the paired resident
      `$EA_INFORMATION` plus resident or nonresident `$EA` copy-on-write,
      commits the new pair before releasing old runs, enforces the Windows
      packed-size ceiling, maintains `NEED_EA`, change time, ARCHIVE, and the
      duplicated packed size/ARCHIVE in every local and parent-index
      `$FILE_NAME`. Reparse points may retain EAs; their `$FILE_NAME` union
      continues to expose the reparse tag instead of an EA size.
      `IRP_MJ_SET_EA` now validates native `FILE_FULL_EA_INFORMATION`, checks
      `FILE_WRITE_EA`, maps zero-length values to deletion, resets query state,
      and the driver advertises `FILE_SUPPORTS_EXTENDED_ATTRIBUTES`. A focused
      image set, replaced case-insensitively, promoted a 32 KiB `NEED_EA`
      value, removed both entries, recovered every allocated cluster, matched
      exact `$EA` bytes through `ntfscat`, matched metadata through `ntfsinfo`,
      and passed `ntfsfix -n`.
- [ ] Complete the rare remaining EA layouts in `$ATTRIBUTE_LIST` files.
      The normal listed layout now creates, replaces, promotes, and removes
      the paired attributes through a fresh durable extension record, publishes
      both list entries together, retires the old record/runs afterward, and
      preserves exact NTFS-3G interoperability. This also fixed owner detection
      for packed resident attributes in the final bytes of a full MFT record.
      Still unsupported are a pre-existing pair split across different owner
      records, multi-segment `$EA` mapping pairs, and removal when a
      `$PROPERTY_SET` or `$LOGGED_UTILITY_STREAM` follows the EA suffix.
      Parent-index, `$Bitmap`, and MFT commits also need LFS transactions for
      crash atomicity.
- [x] `src/filerecord/attributes/findattr.cpp:115,121` — return
      `STATUS_FILE_CORRUPT_ERROR` for absent must-exist attributes; extend the
      always-resident type list beyond `$STANDARD_INFORMATION`.
- [x] Exact bounded resident replacement — validate the complete file-record
      attribute layout, shift the entire following tail on grow/shrink, keep
      8-byte alignment, zero gaps/padding/released bytes, and fail atomically
      when the record has insufficient capacity. Volume-label growth to the
      32-code-unit limit and shrink to empty matched `ntfslabel` and passed
      `ntfsfix -n`; an in-memory test preserves following attributes and
      checks oversized/overflow failure atomicity.
- [x] Resident-to-nonresident `$DATA` promotion when a growing value no longer
      fits in its MFT record. The core preserves the resident payload, allocates
      clusters, emits bounded mapping pairs, zeroes newly initialized gaps,
      protects the remaining tail with initialized-size semantics, updates
      duplicated unnamed-stream sizes, and restores both the MFT record and
      `$Bitmap` allocation on a detected failure.
- [x] `src/filerecord/write/filedata.cpp:33` — decide whether `WriteFileData()`
      should create a missing attribute.
- [x] Bounded nonresident in-place writes — validate size invariants and the
      full run range before the first disk write, reject sparse/compressed/
      encrypted targets, write the correct caller slice across run boundaries,
      preserve byte-count reporting, and commit data/initialized-size changes
      to the owning MFT record. A fresh-image cross-cluster patch matched both
      the shared reader and `ntfscat`, then passed `ntfsfix -n`.
- [x] Ordinary nonresident `$DATA` growth: allocate contiguous-first and then
      bounded fragmented runs from `$Bitmap`, merge and re-encode mapping
      pairs, zero newly initialized logical gaps, preserve uninitialized-tail
      zero-read semantics, update data/initialized sizes, and synchronize every
      local/parent `$FILE_NAME`.
      Fresh-image checks verify exact free-cluster deltas, direct and
      `ntfscat` bytes, parent `$I30` allocation/data sizes, and `ntfsfix -n`.
- [x] Base-record sparse `$DATA` writes and extension. Mapping-pair encoding
      now emits length-only sparse runs without disturbing the previous
      physical LCN. The writer validates the sparse header, compression unit,
      virtual allocation, and physical compressed size; allocates and
      zero-initializes only sparse clusters intersected by the write; preserves
      untouched holes; and synchronizes logical and physical sizes. A sparse
      file created independently through NTFS-3G received two unaligned
      patches, including extension across a hole. Exactly six clusters were
      allocated, direct and `ntfscat` bytes matched, `ntfsinfo` reported
      115,000 logical bytes over 24,576 physical bytes, and `ntfsfix -n`
      remained clean under Release and ASan/UBSan.
- [x] Base-record sparse EOF and Windows sparse-control semantics. Sparse
      shrink/regrowth releases tail runs, preserves initialized-size zeros,
      transitions between ordinary and sparse nonresident headers, and
      demotes an empty stream to resident form. Marking a stream sparse is
      metadata-only; clearing sparse materializes and zeroes every hole, then
      clears the file-wide sparse bit only when no sparse stream remains.
      Zero-data never extends EOF, physically zeroes partial 16-cluster units,
      and converts fully covered units to holes. Allocated-range queries
      coalesce logically adjacent physical extents, bias boundary clusters to
      the exact requested byte range, and preserve Windows overflow behavior.
      ReactOS routes `FSCTL_SET_SPARSE`, `FSCTL_SET_ZERO_DATA`, and
      `FSCTL_QUERY_ALLOCATED_RANGES` through the same shared APIs.
      The independent NTFS-3G fixture verifies exact bytes through both
      readers, sparse and ordinary headers, virtual and physical sizes,
      allocation deltas, all-hole conversion, range-query output, and
      `ntfsfix -n` under Release and ASan/UBSan. Sparse scalar preallocation
      and `$ATTRIBUTE_LIST` extents remain separate work.
- [x] Allocation-layout introspection. A shared retrieval-pointer API validates
      complete run coverage, coalesces adjacent physical or sparse runs,
      rounds a requested VCN back to its containing extent, reports `LCN -1`
      for holes/partial compression units, and supports bounded continuation
      with `STATUS_BUFFER_OVERFLOW`. A shared volume-bitmap API reads literal
      allocation bits from `$Bitmap`, rounds the requested LCN to a byte
      boundary, and returns the total remaining valid-bit count independently
      of output capacity. ReactOS now serves `FSCTL_GET_RETRIEVAL_POINTERS`
      for file, ADS, and nonresident directory streams and
      `FSCTL_GET_VOLUME_BITMAP` for volume handles. The focused fixture matches
      retrieval extents against `ntfscluster`, bitmap bytes against NTFS-3G's
      raw `$Bitmap` reader, and exercises both overflow contracts.
- [x] Native volume and MFT-record introspection. Volume geometry now retains
      the boot sector's exact 64-bit sector and cluster counts, reports
      `$Bitmap` free space, file-record geometry, `$MFT` valid length and
      physical starts, and exposes the NTFS version through one shared API.
      MFT-record enumeration masks the sequence component, clamps oversized
      requests, scans `$MFT::$BITMAP` backward in bounded chunks, returns the
      first in-use ordinal, and copies only a fully validated, fixup-applied
      record. ReactOS serves `FSCTL_GET_NTFS_VOLUME_DATA` (including its
      extended version trailer) and `FSCTL_GET_NTFS_FILE_RECORD` through these
      APIs. The Linux fixture matches geometry/free space with boot-sector and
      NTFS-3G metadata, checks downward enumeration independently against the
      MFT bitmap, and compares returned records with `ntfscat` plus physical
      update-sequence trailers.
- [x] Grow base-owned streams whose mapping pairs require continuation
      attributes in new `$ATTRIBUTE_LIST` extents. Updates split only at run
      boundaries, reset each segment's LCN delta, preserve the authoritative
      sizes in the VCN-zero record, stage unreferenced extension records before
      publishing the ordered list, and remove/deallocate superseded mapping
      records only after the new base/list state is durable. Repeated sparse
      writes forced 128 alternating runs across MFT records 64 and 31; both
      readers matched exactly, truncation to zero collapsed the stream back to
      one resident base attribute, the continuation's `$MFT::$BITMAP` bit
      cleared, and `ntfsfix -n` passed in the single ADS regression.
- [x] Grow extension-owned streams through the same mapping-pair continuation
      transaction. Extension records retain a non-owning link to the loaded
      base record so run reconstruction uses the authoritative
      `$ATTRIBUTE_LIST`; publication writes the VCN-zero owner before the new
      list and preserves that owner while invalidating stale extent caches.
      The compact ADS regression forced 384 alternating mapping segments from
      an extension-owned named stream, verified exact bytes through both
      readers, then collapsed it to one resident extension attribute and
      confirmed the continuation MFT bit was released.
- [x] Grow the nonresident `$ATTRIBUTE_LIST` value within the mapping-pair
      continuation transaction. Newly allocated list runs remain owned by the
      rollback object until the new list is published, the list's mapping
      pairs remain entirely in the base record, and moved target attributes
      are resolved again by stable ID. The compact ADS regression inserted 65
      padding streams to leave a 4,072-byte list in a 4,096-byte allocation;
      one 48-byte continuation entry grew the data to 4,120 bytes and the
      allocation to 8,192 bytes. Exact custom/`ntfscat` bytes, continuation
      collapse, MFT-bit release, `ntfsfix -n`, Release, and ASan/UBSan all
      passed.
- [x] Bound the list's own base-record mapping pairs through copy-on-write
      compaction. Before growth, the core prefers one complete contiguous
      replacement for a fragmented `$ATTRIBUTE_LIST`; it publishes and writes
      the replacement before releasing superseded clusters, while rollback
      releases only the new allocation. If a full contiguous replacement is
      unavailable, bounded incremental growth remains available while the
      resulting mapping pairs fit. The compact ADS regression had NTFS-3G
      build an 8,152-byte list in two physical runs, then one core continuation
      grew it to 8,200 bytes and a 12,288-byte one-run allocation. It verified
      old-cluster release or legitimate reuse through retrieval pointers and
      literal `$Bitmap` state, exact stream bytes, first-attempt `$MFT` growth
      from 196,608 to 209,920 valid bytes, and `ntfsfix -n`. Release,
      ASan/UBSan, and the ReactOS `ntfs.sys` build passed.
- [ ] Complete the remaining attribute-layout edges: the uncommon
      initial-list case with insufficient header room and no
      safely movable resident named stream/security/EA pair, and LFS-backed
      crash atomicity for every multi-record transaction.
- [x] Logical size changes and tail-cluster deallocation for existing ordinary
      unnamed and named streams. Resident values resize in-record or promote;
      nonresident mapping pairs grow/shrink at cluster boundaries, released
      runs are cleared from `$Bitmap` after the new MFT record is durable, and
      zero-length streams return to resident form. ReactOS now serves
      `FileEndOfFileInformation` and allocation shrink through this shared API.
      One fresh image exercised resident shrink/promotion, nonresident
      shrink/uninitialized regrowth/zero demotion, exact free-cluster deltas,
      `$I30` sizes, `ntfscat`, and `ntfsfix -n`.
- [x] Exact `FileAllocationInformation` preallocation independent of EOF.
      Resident streams promote when reservation is requested; nonresident
      mapping pairs grow or shrink while data and initialized sizes stay
      unchanged, and allocation requests below EOF truncate first. A 63-byte
      stream reserved 16 KiB with the same nonresident data/allocation/
      initialized sizes as `ntfsfallocate -n`, then released three clusters
      without changing its bytes. ReactOS routes allocation information
      through the shared primitive.
- [x] Explicit basic-information updates. A portable field-mask API validates
      and writes the common NTFS 1.x/3.x `$STANDARD_INFORMATION` prefix,
      replaces only user-settable attributes, preserves structural flags, and
      supports `FileBasicInformation` through the ReactOS driver with
      `FILE_WRITE_ATTRIBUTES` access. A deterministic four-timestamp and
      attribute update was read back by the core and `ntfsinfo`, left the
      intentionally separate `$FILE_NAME` timestamps unchanged, and passed
      `ntfsfix -n`.
- [x] Automatic timestamp maintenance for ordinary `$DATA` changes.
      Resident, promoted, in-place nonresident, extending, EOF, and named
      stream writes update last-write/change in the same MFT-record commit;
      allocation-only changes update change time without falsifying
      last-write. The portable environment supplies native NT system time.
      ReactOS writable reads maintain last-access and each open handle honors
      `FileBasicInformation` values `-1`/`-2` to suppress/resume individual
      automatic fields; an explicit positive timestamp also suppresses its
      later automatic replacement for that handle. The focused image fixture
      independently checks data-write versus allocation-only semantics and
      passes `ntfsinfo`, `ntfsfix -n`, Release, and ASan/UBSan; `ntfs.sys`
      also builds with the per-handle path enabled.
- [x] `src/mft/get.cpp:72` — stronger file-record validation (attribute 8-byte
      alignment, length sanity) before accepting a record.
- [ ] `src/mft/mft.cpp:69` — add LFS logging around MFT writes. Timestamp
      policy deliberately stays in semantic mutation paths because this
      low-level writer also commits reserved metadata records.
- [x] `src/mft/mft.cpp` (`IsFileRecordNumberInUse`, `#if 0`) — real $Bitmap
      check disabled due to a `disk.c residualOffset == 0` assertion; currently
      always answers "in use". Root-cause the sub-sector read path.
- [x] `src/filerecord/fixup.cpp:39` — USN increment on `CommitFixup()` is
      skipped until LFS journaling exists.
- [ ] `src/filerecord/fixup.cpp:92` — on fixup mismatch, add the sector to
      `$BadClus` and handle instead of `ASSERT`.

### Directories / B-tree
- [x] `src/btree/directory/directory.cpp:274` — honor the `$I30` `$BITMAP`
      attribute in `LoadDirectory()`; `:69` — confirm each index node is marked
      in-use before trusting it.
- [x] `src/btree/directory/directory.cpp:294` — skip the 8.3 marking pass when
      short-name generation is disabled for the volume. Existing on-disk DOS
      aliases must still be recognized even when creation is disabled; the
      namespace-based O(n) pass is therefore retained.
- [x] `src/btree/directory/directory.cpp:370` — `GetShortNameKey()` FIXME:
      dummy-key lookups reach it; rework the calling algorithm.
- [x] `src/btree/directory/editdir.cpp` — `AddFileToDirectory` now inserts
      into the resident root, pushes an overflowing root down into index
      buffer 0 (creating `$BITMAP` plus a promoted `$INDEX_ALLOCATION` in one
      record commit), inserts in place in leaves, splits full nodes around
      their byte median with right-node-first publication, and recursively
      promotes medians along the recorded descent path with root push-down
      at any depth. Publication order keeps every intermediate on-disk state
      readable; crash atomicity across the sequence still waits on LFS.
- [x] `src/btree/directory/editdir.cpp` — `RemoveFileFromDirectory` removes
      entries from the root, leaves, and internal nodes (collation-maximum
      replacement with END-chain release and split-backed overflow);
      `src/mft/namespace.cpp` coordinates names, `$FILE_NAME` attributes,
      link counts, record teardown, and cluster reclamation on top of it.
      Node merging/rebalancing beyond END-chain release stays future work,
      as does crash atomicity (LFS).
- [x] `src/btree/directory/editdir.cpp` — the per-directory bound fell: the
      extension path creates the initial `$ATTRIBUTE_LIST` while headroom
      remains and relocated `$INDEX_ALLOCATION` segments spill to extension
      records; chunked 16-cluster `$MFT` growth keeps record 0 compact.
      5,000 single-directory creations verified with exact independent
      name-set equality and full reclamation on drain.
- [x] `src/btree/directory/get.cpp:189` — revisit the "empty buffer at end of
      directory" detection ("Is there a better way?").
- [x] `src/mft/get.cpp:282` — `GetFileRecordFromQuery()` `"//folder//target//"`
      double-separator handling marked "Is this a hack or fix?" — decide and
      document.

### LFS / journaling (the big one)
- [ ] `include/internal/ntfslib_new_internal.h:902` — roadmap comment:
      `LogTransaction()` / `CommitTransaction()` / periodic
      `WriteCheckpointRecord()` (declarations were removed until they exist).
- [x] `src/lfs/lfs.cpp:67` — pick the best restart page by corruption + LSN
      recency instead of always page 1; honor `SystemPageSize` from the page
      header instead of the hardcoded 4096 offset.
- [x] Recognize a completely all-`0xff` `$LogFile` as NTFS's empty/clean
      journal representation, matching NTFS-3G. The check scans the full
      stream in bounded chunks so a corrupt nonempty tail cannot be ignored.
- [ ] `src/lfs/lfs.cpp` — `PerformFileSystemRecovery()` / `ShutdownLFS()` are
      stubs.
- [ ] `include/internal/ntfslib_new_internal.h:686` — determine which
      `RestartArea` variant (NT4 / 2000-2003 / Vista+) applies at runtime.
- [x] `env/km/km.cpp:43` — validate NTFS-legal sector sizes in
      `NtfsDiskInitializeKm()`.
- [x] `env/km/km.cpp:50,101,173` — the `ByPasscompletion` routine "shouldn't be
      needed"; something wrong with IRP completion (`Iosb.Status` is never
      consulted after the wait). Root-cause and remove.
- [x] `include/public/ntfsattribdef.h:212` — complete the
      `$SECURITY_DESCRIPTOR` on-disk structure.

## 4. Performance follow-ups (flagged by review, intentionally not done)

- [x] Lazy B-tree descent for exact opens: pathname lookup scans the resident
      `$INDEX_ROOT`, then reads only the `$INDEX_ALLOCATION` buffers selected
      by filename collation. Every child is checked against `$BITMAP`, its INDX
      fixup/signature/VCN are validated, and duplicate/cyclic VCN traversal is
      rejected. Enumeration deliberately retains the sequential bulk-read
      path. Mixed-case nested lookup and left/middle/right paths in a
      multi-level 1,001-entry directory matched ntfs-3g tools.
- [x] Bulk `$I30` enumeration: read `$INDEX_ALLOCATION` and its bitmap once,
      apply fixups in the owned buffer, reject duplicate/cyclic child VCNs,
      point keys directly at validated entries, and allocate key objects in
      fixed-size arenas. Directories without DOS aliases skip the alias hash
      passes. The current mountless comparison and remaining startup gap are
      recorded below; older point samples are not retained as performance
      claims.
- [x] 8.3 handling via the namespace byte: `FileNameEx.NameType`
      (`NAME_TYPE_DOS` / `WIN32_AND_DOS`, defined but unused) is the on-disk
      truth. One O(n) marking pass replaces the current O(n²) heuristic scan in
      `LoadDirectory()`; storing the discovered pairing (a `ShortNameKey` field
      in `_BTreeKey`) makes enumeration O(1) per entry instead of re-walking the
      tail in `GetFileBothDirInfo()`.
- [x] Collation early-out for exact lookup: entries are sorted, so stop
      scanning once the target sorts before the current key and descend only
      through that entry's child. Comparisons now use the volume's cached
      `$UpCase` table rather than host NLS rules.
- [x] Metadata singleton caching umbrella: `$AttrDef` (parse once into a small
      (type, name) table at mount) and the volume label (cache;
      `SetVolumeLabel` is the single writer) still re-fetch per query. `$UpCase`
      and `$MFT`/`$MFTMirr` runs are done.
- [x] Write path still re-decodes `$MFT` runs per write
      (`WriteFileRecordToMFT` → `WriteFileData` → `UpdateNonResidentData`);
      plumb the cached run list once the write path stabilizes.
- [ ] `env/km` I/O: reusable/lookaside bounce buffer instead of per-call pool
      alloc; unaligned writes currently read-modify-write the FULL span — only
      the partial head/tail sectors need pre-reading, the aligned middle can be
      written directly from the caller's buffer.
- [x] Pass the volume `$UpCase` table (now cached in `Volume`) as the
      `UpcaseTable` argument to `NtfsIsNameInExpression()` so name matching
      uses volume collation instead of system NLS (see note in
      `DoesFileNameMatch()`); needs the um fallback implemented first (§3).

### Compact Linux core comparison

One optimized Release sample on 2026-07-24 used mountless commands against a
384 MiB NTFS 3.1 image with 250 small files and one 64 MiB uncompressed
stream. Both tools returned the same 251 names and exact stream bytes. After
one untimed warm-up per tool, one coordinator pinned to CPU 0 ran 100 fresh
directory commands and three fresh stream-read commands. Each command creates
a new process and volume object while the host page cache remains warm; the
reported time includes identical Python process-launch overhead for both
implementations.

- Fresh open plus directory enumeration: ntfslib 2,051.35 ops/s; NTFS-3G
  (`ntfsls`) 1,118.03 ops/s. ntfslib is 1.84x faster.
- Fresh open plus cached sequential read: ntfslib 9,885.20 MiB/s; NTFS-3G
  (`ntfscat`) 4,834.32 MiB/s. ntfslib is 2.05x faster.
- This is intentionally one small regression sample, not a statistical claim.
  It is the current direct-core baseline.

The workload is now scripted as `linux/tests/benchmark_core.sh`: it builds
the shared fixture, refuses to time until both tools return identical names
and bytes, pins every timed launch with `taskset`, alternates tool order
across at least five samples, emits raw CSV plus medians, never mounts, and
preserves the fixture only on failure. The post-creation-tranche rerun on
2026-07-24 (same machine in `powersave` at a 0.8 GHz floor, ~1.0 background
load, `NTFSLIB_BENCH_CPU=12`, ntfs-3g 2022.10.3) reported medians of 788.14
versus 566.01 ops/s on directory enumeration (1.39x) and 4,398.32 versus
3,391.45 MiB/s on cached sequential reads (1.30x). After the namespace
tranche (deletion, rename, hard links, attribute-list wiring, chunked
`$MFT` growth) a nine-sample run under the same governor reported medians
of 337.95 versus 270.55 ops/s on directory enumeration (1.25x) and
2,813.39 versus 2,013.80 MiB/s on cached reads (1.40x); the lower
absolutes track a post-build thermal sag, not a code change. Absolute
rates are frequency-limited and not comparable with the boosted baseline
above; the direction is unchanged in every sample set, and a CPU 0 run
under the same governor showed launch-noise variance large enough to flip
the directory ratio, so samples must record governor and pin conditions.

Callgrind and syscall profiling had put about 75% of the dynamically linked
ntfslib listing's retired instructions in the loader and C++ runtime, while
ntfslib made 16 image `pread` calls versus 38 for `ntfsls`. The GNU Linux
inspection executable now links its compiler-support runtime statically by
default, so the baseline does not charge shared-core `$I30` work for that
unrelated loader cost. `NTFSLIB_STATIC_CXX_RUNTIME=OFF` restores dynamic
compiler runtimes when package size is more important than cold startup.

For frontend-only context, the earlier FUSE sample reported 29,443.83 versus
14,905.50 directory operations/s, direct-read parity, and a 1.61x ntfs-3g
cached-read lead. The ntfslib FUSE frontend caches complete directory
listings, and the Linux kernel controls the cached-read path, so those rows
must not be used as evidence that one NTFS core is faster.

## 5. Cleanup follow-ups (nice-to-have, low risk)

- [ ] Dedup the shared tail of `GetFileRecord()` / `GetFileRecordFromMFTMirr()`
      (validate-magic + fixup + error mapping) — careful: magic-failure falls
      back to `$MFTMirr`, fixup-failure must not; encode that explicitly.
- [x] One `ReadAttributeAlloc(Attr, &Buffer, &Bytes)` helper for the two
      remaining "size → new → CopyData → subtract" sites (attrdef, upcase),
      burying the `*Length`-is-remaining idiom in one place.
- [x] Unify the three USA/fixup walkers (`Directory::VerifyUpdateSequenceArray`
      — which isn't really a Directory method — and
      `FileRecord::CommitFixup`/`ApplyFixup`) into one free helper over
      `PNTFSRecordHeader` + block size.
- [ ] Convert the member-capturing macros (`BytesPerCluster`, `GetOffset`,
      `GetRunSize`, `FileRecordOffset`, `IsFileRecordInMFTMirr`,
      `GetAllocationOffsetFromVCN`, `MFTDiskOffset`/`MFTMirrDiskOffset`) into
      inline member functions on `Volume`/`MasterFileTable` — they silently
      capture `DiskVolume`/`FileRecordSize` from ambient scope.
- [x] Allocator consistency: `directory.cpp` mixes raw `NtfsAllocatePoolWithTag`
      with `delete`, and `new(pool) UCHAR[n]` arrays are freed with scalar
      `delete` via `DestroyBTreeKey`; settle on the placement-`new`/`delete[]`
      pairing everywhere.
- [ ] Move the ~330-line LFS/UsnJrnl on-disk struct block out of
      `ntfslib_new_internal.h` into `include/internal/lfs_ondisk.h` (restores
      the old `lfs/logfile.h` split; most of it is unreferenced reference
      material).
- [x] `PathElementLength` macro → helper function returning the element length
      once (currently up to 3 `wcschr`/`wcslen` per expansion, re-expanded per
      node key via `CompareLength`) — fold into the §1 chars/bytes fix.
- [x] A tiny helper for the four hand-rolled counted-`UNICODE_STRING`
      constructions (dbg.cpp, directory.cpp, find.cpp ×2).
- [x] `src/btree/directory/dbg.cpp` (`DumpFileTree`) has no callers — keep as
      bringup tooling or delete before release.
