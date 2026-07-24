/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS compressed-stream reader
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

#define NTFS_LZNT1_CHUNK_SIZE 4096
#define NTFS_COMPRESSION_UNIT_SHIFT 4

#ifdef NTFSLIB_PORTABLE
/*
 * Decode the LZNT1 byte stream used by ordinary NTFS compression. Chunks are
 * independent and produce at most 4 KiB, so a back-reference can never escape
 * the current chunk.
 */
static NTSTATUS
NtfsDecompressLznt1(
    _Out_ PUCHAR Output,
    _In_ ULONG OutputSize,
    _In_ PUCHAR Input,
    _In_ ULONG InputSize,
    _Out_ PULONG FinalSize)
{
    ULONG InputOffset = 0;
    ULONG OutputOffset = 0;

    *FinalSize = 0;

    while (InputOffset + sizeof(USHORT) <= InputSize &&
           OutputOffset < OutputSize)
    {
        USHORT Header;
        ULONG ChunkInputSize;
        ULONG ChunkOutputLimit;
        ULONG ChunkOutputSize = 0;
        PUCHAR ChunkInput;
        PUCHAR ChunkInputEnd;

        Header = (USHORT)Input[InputOffset] |
                 ((USHORT)Input[InputOffset + 1] << 8);
        InputOffset += sizeof(USHORT);

        if (Header == 0)
            break;
        if ((Header & 0x7000) != 0x3000)
            return STATUS_FILE_CORRUPT_ERROR;

        ChunkInputSize = (Header & 0x0fff) + 1;
        if (ChunkInputSize > InputSize - InputOffset)
            return STATUS_FILE_CORRUPT_ERROR;

        ChunkInput = Input + InputOffset;
        ChunkInputEnd = ChunkInput + ChunkInputSize;
        ChunkOutputLimit = min((ULONG)NTFS_LZNT1_CHUNK_SIZE,
                               OutputSize - OutputOffset);

        if (!(Header & 0x8000))
        {
            if (ChunkInputSize > ChunkOutputLimit)
                return STATUS_FILE_CORRUPT_ERROR;

            RtlCopyMemory(Output + OutputOffset,
                          ChunkInput,
                          ChunkInputSize);
            ChunkOutputSize = ChunkInputSize;
        }
        else
        {
            PUCHAR Cursor = ChunkInput;

            while (Cursor < ChunkInputEnd &&
                   ChunkOutputSize < ChunkOutputLimit)
            {
                UCHAR Flags = *Cursor++;
                ULONG TokenIndex;

                for (TokenIndex = 0;
                     TokenIndex < 8 &&
                     Cursor < ChunkInputEnd &&
                     ChunkOutputSize < ChunkOutputLimit;
                     TokenIndex++)
                {
                    if (Flags & (1 << TokenIndex))
                    {
                        USHORT Token;
                        ULONG DisplacementBits;
                        ULONG LengthBits;
                        ULONG LengthMask;
                        ULONG Displacement;
                        ULONG Length;

                        if ((ULONG)(ChunkInputEnd - Cursor) <
                            sizeof(USHORT))
                        {
                            return STATUS_FILE_CORRUPT_ERROR;
                        }

                        Token = (USHORT)Cursor[0] |
                                ((USHORT)Cursor[1] << 8);
                        Cursor += sizeof(USHORT);

                        for (DisplacementBits = 12;
                             DisplacementBits > 4;
                             DisplacementBits--)
                        {
                            if ((1UL << (DisplacementBits - 1)) <
                                ChunkOutputSize)
                            {
                                break;
                            }
                        }

                        LengthBits = 16 - DisplacementBits;
                        LengthMask = (1UL << LengthBits) - 1;
                        Length = (Token & LengthMask) + 3;
                        Displacement = (Token >> LengthBits) + 1;

                        if (Displacement > ChunkOutputSize ||
                            Length > ChunkOutputLimit - ChunkOutputSize)
                        {
                            return STATUS_FILE_CORRUPT_ERROR;
                        }

                        while (Length--)
                        {
                            Output[OutputOffset + ChunkOutputSize] =
                                Output[OutputOffset + ChunkOutputSize -
                                       Displacement];
                            ChunkOutputSize++;
                        }
                    }
                    else
                    {
                        Output[OutputOffset + ChunkOutputSize++] = *Cursor++;
                    }
                }
            }
        }

        OutputOffset += ChunkOutputSize;
        InputOffset += ChunkInputSize;
    }

    *FinalSize = OutputOffset;
    return STATUS_SUCCESS;
}
#else
static NTSTATUS
NtfsDecompressLznt1(
    _Out_ PUCHAR Output,
    _In_ ULONG OutputSize,
    _In_ PUCHAR Input,
    _In_ ULONG InputSize,
    _Out_ PULONG FinalSize)
{
    return RtlDecompressBuffer(COMPRESSION_FORMAT_LZNT1,
                               Output,
                               OutputSize,
                               Input,
                               InputSize,
                               FinalSize);
}
#endif

static NTSTATUS
ReadCompressionUnit(
    _In_ PVolume DiskVolume,
    _In_ PDataRun Runs,
    _In_ ULONGLONG UnitStartVCN,
    _In_ ULONGLONG UnitClusters,
    _In_ ULONG RequiredBytes,
    _Out_ PUCHAR StoredBuffer,
    _Out_ PUCHAR UnitBuffer,
    _In_ ULONG UnitBytes)
{
    const ULONGLONG MaximumValue = ~(ULONGLONG)0;
    PDataRun Run = Runs;
    ULONGLONG RunStartVCN = 0;
    ULONGLONG UnitEndVCN;
    ULONGLONG MappedClusters = 0;
    ULONGLONG StoredClusters = 0;
    ULONGLONG ClusterSize = BytesPerCluster(DiskVolume);
    ULONGLONG RequiredClusters;
    BOOLEAN SawSparse = FALSE;
    NTSTATUS Status;

    if (UnitStartVCN > MaximumValue - UnitClusters)
        return STATUS_FILE_CORRUPT_ERROR;
    UnitEndVCN = UnitStartVCN + UnitClusters;
    RequiredClusters = RequiredBytes / ClusterSize;
    if (RequiredBytes % ClusterSize)
        RequiredClusters++;

    RtlZeroMemory(UnitBuffer, UnitBytes);

    while (Run)
    {
        if (RunStartVCN > MaximumValue - Run->Length)
            return STATUS_FILE_CORRUPT_ERROR;
        if (RunStartVCN + Run->Length > UnitStartVCN)
            break;

        RunStartVCN += Run->Length;
        Run = Run->NextRun;
    }

    while (Run && RunStartVCN < UnitEndVCN)
    {
        ULONGLONG RunEndVCN;
        ULONGLONG OverlapStart;
        ULONGLONG OverlapEnd;
        ULONGLONG ClusterCount;

        if (RunStartVCN > MaximumValue - Run->Length)
            return STATUS_FILE_CORRUPT_ERROR;
        RunEndVCN = RunStartVCN + Run->Length;
        OverlapStart = RunStartVCN < UnitStartVCN
            ? UnitStartVCN
            : RunStartVCN;
        OverlapEnd = RunEndVCN < UnitEndVCN
            ? RunEndVCN
            : UnitEndVCN;
        ClusterCount = OverlapEnd - OverlapStart;

        if (ClusterCount != 0)
        {
            if (Run->IsSparse)
            {
                SawSparse = TRUE;
            }
            else
            {
                ULONGLONG RunClusterOffset;
                ULONGLONG ReadLCN;
                ULONGLONG ReadBytes;
                ULONGLONG BufferOffset;

                /* Allocated clusters must precede sparse padding in a unit. */
                if (SawSparse)
                    return STATUS_FILE_CORRUPT_ERROR;

                RunClusterOffset = OverlapStart - RunStartVCN;
                if (Run->LCN > MaximumValue - RunClusterOffset)
                    return STATUS_FILE_CORRUPT_ERROR;
                ReadLCN = Run->LCN + RunClusterOffset;
                ReadBytes = ClusterCount * ClusterSize;
                BufferOffset = StoredClusters * ClusterSize;
                if (ReadBytes > MAXULONG ||
                    BufferOffset > UnitBytes ||
                    ReadBytes > UnitBytes - BufferOffset)
                {
                    return STATUS_FILE_CORRUPT_ERROR;
                }

                Status = DiskVolume->ReadVolume(
                    GetOffset(ReadLCN),
                    (ULONG)ReadBytes,
                    StoredBuffer + BufferOffset);
                if (!NT_SUCCESS(Status))
                    return Status;

                StoredClusters += ClusterCount;
            }

            MappedClusters += ClusterCount;
        }

        RunStartVCN = RunEndVCN;
        Run = Run->NextRun;
    }

    if (MappedClusters < RequiredClusters)
        return STATUS_FILE_CORRUPT_ERROR;

    if (StoredClusters == 0)
        return STATUS_SUCCESS;

    if (!SawSparse)
    {
        ULONGLONG StoredBytes = StoredClusters * ClusterSize;

        if (StoredBytes > UnitBytes || StoredBytes < RequiredBytes)
            return STATUS_FILE_CORRUPT_ERROR;
        RtlCopyMemory(UnitBuffer, StoredBuffer, (ULONG)StoredBytes);
        return STATUS_SUCCESS;
    }

    {
        ULONGLONG StoredBytes = StoredClusters * ClusterSize;
        ULONG FinalSize;

        if (StoredBytes == 0 || StoredBytes > UnitBytes)
            return STATUS_FILE_CORRUPT_ERROR;

        Status = NtfsDecompressLznt1(UnitBuffer,
                                     UnitBytes,
                                     StoredBuffer,
                                     (ULONG)StoredBytes,
                                     &FinalSize);
        if (!NT_SUCCESS(Status) ||
            FinalSize > UnitBytes ||
            FinalSize < RequiredBytes)
            return STATUS_FILE_CORRUPT_ERROR;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
FileRecord::CopyCompressedData(
    _In_ PAttribute Attribute,
    _In_ PUCHAR Buffer,
    _Inout_ PULONG Length,
    _In_ ULONGLONG Offset)
{
    PDataRun Runs;
    PUCHAR StoredBuffer = NULL;
    PUCHAR UnitBuffer = NULL;
    ULONGLONG ClusterSize;
    ULONGLONG UnitClusters;
    ULONGLONG UnitBytes64;
    ULONG UnitBytes;
    ULONG BytesToRead;
    ULONG BytesFromRuns;
    ULONG BytesRead = 0;
    NTSTATUS Status = STATUS_SUCCESS;

    if (!Attribute ||
        !Attribute->IsNonResident ||
        !(Attribute->Flags & ATTR_COMPRESSED) ||
        Attribute->Flags & ATTR_ENCRYPTED)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Attribute->NonResident.CompressionUnitSize !=
            NTFS_COMPRESSION_UNIT_SHIFT ||
        Attribute->Length < 0x48 ||
        Attribute->NonResident.DataRunsOffset < 0x48 ||
        Attribute->NonResident.InitalizedDataSize >
            Attribute->NonResident.DataSize)
    {
        return STATUS_FILE_CORRUPT_ERROR;
    }

    if (Offset >= Attribute->NonResident.DataSize)
        return STATUS_END_OF_FILE;

    BytesToRead = (ULONG)min(
        Attribute->NonResident.DataSize - Offset,
        (ULONGLONG)*Length);
    BytesFromRuns = Offset < Attribute->NonResident.InitalizedDataSize
        ? (ULONG)min((ULONGLONG)BytesToRead,
                     Attribute->NonResident.InitalizedDataSize - Offset)
        : 0;

    if (BytesFromRuns == 0)
    {
        RtlZeroMemory(Buffer, BytesToRead);
        *Length -= BytesToRead;
        return STATUS_SUCCESS;
    }

    ClusterSize = BytesPerCluster(DiskVolume);
    UnitClusters = 1ULL << NTFS_COMPRESSION_UNIT_SHIFT;
    if (ClusterSize == 0 ||
        ClusterSize > NTFS_LZNT1_CHUNK_SIZE ||
        UnitClusters > MAXULONG / ClusterSize)
    {
        return STATUS_NOT_IMPLEMENTED;
    }

    UnitBytes64 = UnitClusters * ClusterSize;
    UnitBytes = (ULONG)UnitBytes64;
    Runs = GetCachedDataRuns(Attribute);
    if (!Runs)
        return STATUS_FILE_CORRUPT_ERROR;

    StoredBuffer = new(PagedPool, TAG_NTFS) UCHAR[UnitBytes];
    UnitBuffer = new(PagedPool, TAG_NTFS) UCHAR[UnitBytes];
    if (!StoredBuffer || !UnitBuffer)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }

    while (BytesRead < BytesFromRuns)
    {
        ULONGLONG AbsoluteOffset = Offset + BytesRead;
        ULONGLONG UnitIndex = AbsoluteOffset / UnitBytes64;
        ULONGLONG UnitStartOffset = UnitIndex * UnitBytes64;
        ULONGLONG UnitStartVCN = UnitIndex * UnitClusters;
        ULONGLONG RequiredBytes64;
        ULONG RequiredBytes;
        ULONG OffsetInUnit = (ULONG)(AbsoluteOffset - UnitStartOffset);
        ULONG Chunk = min(BytesFromRuns - BytesRead,
                          UnitBytes - OffsetInUnit);

        if (UnitStartOffset >=
            Attribute->NonResident.InitalizedDataSize)
        {
            Status = STATUS_FILE_CORRUPT_ERROR;
            goto Done;
        }

        RequiredBytes64 =
            Attribute->NonResident.InitalizedDataSize - UnitStartOffset;
        if (RequiredBytes64 > UnitBytes)
            RequiredBytes64 = UnitBytes;
        RequiredBytes = (ULONG)RequiredBytes64;

        Status = ReadCompressionUnit(DiskVolume,
                                     Runs,
                                     UnitStartVCN,
                                     UnitClusters,
                                     RequiredBytes,
                                     StoredBuffer,
                                     UnitBuffer,
                                     UnitBytes);
        if (!NT_SUCCESS(Status))
            goto Done;

        RtlCopyMemory(Buffer + BytesRead,
                      UnitBuffer + OffsetInUnit,
                      Chunk);
        BytesRead += Chunk;
    }

    if (BytesRead < BytesToRead)
    {
        RtlZeroMemory(Buffer + BytesRead, BytesToRead - BytesRead);
        BytesRead = BytesToRead;
    }

    *Length -= BytesRead;

Done:
    delete[] UnitBuffer;
    delete[] StoredBuffer;
    return Status;
}
