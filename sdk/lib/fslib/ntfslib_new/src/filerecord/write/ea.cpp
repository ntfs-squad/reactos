/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Atomic native NTFS extended-attribute list updates
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

#define NTFS_MAX_PACKED_EA_SIZE ((ULONG)0xfffb)

typedef struct _EA_BUILD_ENTRY
{
    UINT8 Flags;
    UINT8 NameLength;
    UINT16 ValueLength;
    const UCHAR* Name;
    const UCHAR* Value;
    BOOLEAN Active;
} EA_BUILD_ENTRY, *PEA_BUILD_ENTRY;

static UCHAR
UpperEaCharacter(_In_ UCHAR Character)
{
    if (Character >= 'a' && Character <= 'z')
        return Character - ('a' - 'A');
    return Character;
}

static BOOLEAN
EaNamesEqual(_In_ const UCHAR* Left,
             _In_ UINT8 LeftLength,
             _In_ const UCHAR* Right,
             _In_ UINT8 RightLength)
{
    if (LeftLength != RightLength)
        return FALSE;

    for (ULONG Index = 0; Index < LeftLength; Index++)
    {
        if (UpperEaCharacter(Left[Index]) !=
            UpperEaCharacter(Right[Index]))
        {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOLEAN
IsEaNameCharacterValid(_In_ UCHAR Character)
{
    if (Character < 0x20 || Character > 0x7e)
        return FALSE;

    switch (Character)
    {
        case '\\':
        case '/':
        case ':':
        case '*':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
        case ',':
        case '+':
        case '=':
        case '[':
        case ']':
        case ';':
            return FALSE;

        default:
            return TRUE;
    }
}

static NTSTATUS
ValidateEaUpdates(
    _In_reads_(UpdateCount)
        const NtfsExtendedAttributeUpdate* Updates,
    _In_ ULONG UpdateCount)
{
    if (UpdateCount != 0 && !Updates)
        return STATUS_INVALID_PARAMETER;

    for (ULONG UpdateIndex = 0;
         UpdateIndex < UpdateCount;
         UpdateIndex++)
    {
        const NtfsExtendedAttributeUpdate* Update =
            &Updates[UpdateIndex];

        if (Update->Operation != NTFS_EA_UPDATE_SET &&
            Update->Operation != NTFS_EA_UPDATE_REMOVE)
        {
            return STATUS_INVALID_PARAMETER;
        }
        if (!Update->Name ||
            Update->NameLength == 0 ||
            Update->NameLength == 0xff)
        {
            return STATUS_INVALID_EA_NAME;
        }
        for (ULONG NameIndex = 0;
             NameIndex < Update->NameLength;
             NameIndex++)
        {
            if (!IsEaNameCharacterValid(
                    Update->Name[NameIndex]))
            {
                return STATUS_INVALID_EA_NAME;
            }
        }

        if (Update->Operation == NTFS_EA_UPDATE_SET)
        {
            if (Update->Flags &
                    ~NTFS_EA_FLAG_NEED_EA ||
                (Update->ValueLength != 0 &&
                 !Update->Value))
            {
                return STATUS_INVALID_EA_NAME;
            }
        }
    }
    return STATUS_SUCCESS;
}

static NTSTATUS
LoadExistingExtendedAttributes(
    _In_ PFileRecord File,
    _Outptr_result_bytebuffer_(*RawLength)
        PUCHAR* RawData,
    _Out_ PULONG RawLength,
    _Out_ PEAInformationEx Information,
    _Out_ PULONG EntryCount)
{
    NtfsExtendedAttributeView View;
    PUCHAR Buffer = NULL;
    ULONG Length = 0;
    ULONG Offset = 0;
    ULONG Count = 0;
    NTSTATUS Status;

    if (!File || !RawData || !RawLength ||
        !Information || !EntryCount)
    {
        return STATUS_INVALID_PARAMETER;
    }
    *RawData = NULL;
    *RawLength = 0;
    *EntryCount = 0;
    RtlZeroMemory(Information, sizeof(*Information));

    Status = File->ReadExtendedAttributes(NULL,
                                          &Length,
                                          Information);
    if (Status == STATUS_NO_EAS_ON_FILE)
        return STATUS_SUCCESS;
    if (Status != STATUS_BUFFER_TOO_SMALL)
        return Status;
    if (Length == 0)
        return STATUS_EA_CORRUPT_ERROR;

    Buffer = new(PagedPool, TAG_NTFS) UCHAR[Length];
    if (!Buffer)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = File->ReadExtendedAttributes(Buffer,
                                          &Length,
                                          Information);
    if (!NT_SUCCESS(Status))
        goto Done;

    while (Offset < Length)
    {
        Status = NtfsGetNextExtendedAttribute(Buffer,
                                               Length,
                                               &Offset,
                                               &View);
        if (!NT_SUCCESS(Status))
            goto Done;
        if (Count == MAXULONG)
        {
            Status = STATUS_EA_CORRUPT_ERROR;
            goto Done;
        }
        Count++;
    }
    if (Offset != Length || Count == 0)
    {
        Status = STATUS_EA_CORRUPT_ERROR;
        goto Done;
    }

    *RawData = Buffer;
    *RawLength = Length;
    *EntryCount = Count;
    Buffer = NULL;
    Status = STATUS_SUCCESS;

Done:
    delete[] Buffer;
    return Status;
}

static ULONG
FindEaBuildEntry(_In_ PEA_BUILD_ENTRY Entries,
                 _In_ ULONG EntryCount,
                 _In_ const UCHAR* Name,
                 _In_ UINT8 NameLength)
{
    for (ULONG Index = 0; Index < EntryCount; Index++)
    {
        if (Entries[Index].Active &&
            EaNamesEqual(Entries[Index].Name,
                         Entries[Index].NameLength,
                         Name,
                         NameLength))
        {
            return Index;
        }
    }
    return MAXULONG;
}

static NTSTATUS
PopulateEaBuildEntries(
    _In_opt_ const UCHAR* ExistingData,
    _In_ ULONG ExistingLength,
    _In_ ULONG ExistingCount,
    _In_reads_(UpdateCount)
        const NtfsExtendedAttributeUpdate* Updates,
    _In_ ULONG UpdateCount,
    _Out_ PEA_BUILD_ENTRY Entries,
    _Out_ PULONG EntryCount)
{
    NtfsExtendedAttributeView View;
    ULONG Count = 0;
    ULONG ExistingOffset = 0;
    ULONG Match;
    NTSTATUS Status;

    if (!Entries || !EntryCount)
        return STATUS_INVALID_PARAMETER;
    *EntryCount = 0;

    while (ExistingOffset < ExistingLength)
    {
        Status = NtfsGetNextExtendedAttribute(
            ExistingData,
            ExistingLength,
            &ExistingOffset,
            &View);
        if (!NT_SUCCESS(Status))
            return Status;
        if (Count >= ExistingCount ||
            FindEaBuildEntry(Entries,
                             Count,
                             View.Name,
                             View.NameLength) != MAXULONG)
        {
            return STATUS_EA_CORRUPT_ERROR;
        }

        Entries[Count].Flags = View.Flags;
        Entries[Count].NameLength = View.NameLength;
        Entries[Count].ValueLength = View.ValueLength;
        Entries[Count].Name = View.Name;
        Entries[Count].Value = View.Value;
        Entries[Count].Active = TRUE;
        Count++;
    }
    if (Count != ExistingCount)
        return STATUS_EA_CORRUPT_ERROR;

    /*
     * Windows applies a FILE_FULL_EA_INFORMATION list in order: remove the
     * old name, then add the supplied nonempty value. Keeping that ordering
     * also gives duplicate names in one batch their native "last wins"
     * behavior.
     */
    for (ULONG UpdateIndex = 0;
         UpdateIndex < UpdateCount;
         UpdateIndex++)
    {
        const NtfsExtendedAttributeUpdate* Update =
            &Updates[UpdateIndex];

        Match = FindEaBuildEntry(Entries,
                                 Count,
                                 Update->Name,
                                 Update->NameLength);
        if (Update->Operation == NTFS_EA_UPDATE_REMOVE)
        {
            if (Match != MAXULONG)
                Entries[Match].Active = FALSE;
            continue;
        }

        if (Match == MAXULONG)
        {
            Match = Count++;
            Entries[Match].Name = Update->Name;
            Entries[Match].NameLength =
                Update->NameLength;
            Entries[Match].Active = TRUE;
        }
        Entries[Match].Flags = Update->Flags;
        Entries[Match].ValueLength =
            Update->ValueLength;
        Entries[Match].Value = Update->Value;
    }

    *EntryCount = Count;
    return STATUS_SUCCESS;
}

static NTSTATUS
BuildRawExtendedAttributes(
    _In_ PEA_BUILD_ENTRY Entries,
    _In_ ULONG EntryCount,
    _Outptr_result_bytebuffer_(*RawLength)
        PUCHAR* RawData,
    _Out_ PULONG RawLength,
    _Out_ PEAInformationEx Information)
{
    EAEx Header;
    PUCHAR Buffer = NULL;
    ULONG EntryLength;
    ULONG NeedEaCount = 0;
    ULONG Offset = 0;
    ULONG PackedEntryLength;
    ULONG PackedLength = 0;
    ULONG RawSize = 0;

    if (!Entries || !RawData || !RawLength ||
        !Information)
    {
        return STATUS_INVALID_PARAMETER;
    }
    *RawData = NULL;
    *RawLength = 0;
    RtlZeroMemory(Information, sizeof(*Information));

    for (ULONG Index = 0; Index < EntryCount; Index++)
    {
        if (!Entries[Index].Active)
            continue;

        PackedEntryLength =
            5UL +
            Entries[Index].NameLength +
            Entries[Index].ValueLength;
        if (PackedEntryLength >
                NTFS_MAX_PACKED_EA_SIZE ||
            PackedLength >
                NTFS_MAX_PACKED_EA_SIZE -
                    PackedEntryLength)
        {
            return STATUS_EA_TOO_LARGE;
        }
        PackedLength += PackedEntryLength;

        EntryLength = ALIGN_UP_BY(
            sizeof(EAEx) +
                Entries[Index].NameLength +
                1UL +
                Entries[Index].ValueLength,
            sizeof(ULONG));
        if (EntryLength >
                MAXULONG - RawSize)
        {
            return STATUS_EA_TOO_LARGE;
        }
        RawSize += EntryLength;

        if (Entries[Index].Flags &
            NTFS_EA_FLAG_NEED_EA)
        {
            if (NeedEaCount == MAXUSHORT)
                return STATUS_EA_TOO_LARGE;
            NeedEaCount++;
        }
    }

    if (RawSize == 0)
        return STATUS_SUCCESS;

    Buffer = new(PagedPool, TAG_NTFS) UCHAR[RawSize];
    if (!Buffer)
        return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(Buffer, RawSize);

    for (ULONG Index = 0; Index < EntryCount; Index++)
    {
        if (!Entries[Index].Active)
            continue;

        EntryLength = ALIGN_UP_BY(
            sizeof(EAEx) +
                Entries[Index].NameLength +
                1UL +
                Entries[Index].ValueLength,
            sizeof(ULONG));
        RtlZeroMemory(&Header, sizeof(Header));
        Header.OffsetNextEA = EntryLength;
        Header.Flags = Entries[Index].Flags;
        Header.NameLength =
            Entries[Index].NameLength;
        Header.ValueLength =
            Entries[Index].ValueLength;
        RtlCopyMemory(Buffer + Offset,
                      &Header,
                      sizeof(Header));
        RtlCopyMemory(Buffer + Offset + sizeof(Header),
                      Entries[Index].Name,
                      Entries[Index].NameLength);
        if (Entries[Index].ValueLength != 0)
        {
            RtlCopyMemory(
                Buffer + Offset + sizeof(Header) +
                    Entries[Index].NameLength + 1,
                Entries[Index].Value,
                Entries[Index].ValueLength);
        }
        Offset += EntryLength;
    }

    if (Offset != RawSize)
    {
        delete[] Buffer;
        return STATUS_EA_CORRUPT_ERROR;
    }

    Information->PackedEASize =
        (USHORT)PackedLength;
    Information->NumEAWithNEED_EA =
        (USHORT)NeedEaCount;
    Information->UnpackedEASize = RawSize;
    *RawData = Buffer;
    *RawLength = RawSize;
    return STATUS_SUCCESS;
}

static BOOLEAN
ExtendedAttributesAreIdentical(
    _In_opt_ const UCHAR* LeftData,
    _In_ ULONG LeftLength,
    _In_ const EAInformationEx* LeftInformation,
    _In_opt_ const UCHAR* RightData,
    _In_ ULONG RightLength,
    _In_ const EAInformationEx* RightInformation)
{
    if (LeftLength != RightLength ||
        RtlCompareMemory(LeftInformation,
                         RightInformation,
                         sizeof(*LeftInformation)) !=
            sizeof(*LeftInformation))
    {
        return FALSE;
    }
    return LeftLength == 0 ||
           RtlCompareMemory(LeftData,
                            RightData,
                            LeftLength) == LeftLength;
}

NTSTATUS
FileRecord::UpdateExtendedAttributes(
    _In_reads_(UpdateCount)
        const NtfsExtendedAttributeUpdate* Updates,
    _In_ ULONG UpdateCount)
{
    PEA_BUILD_ENTRY Entries = NULL;
    PDataRun OldEaRuns = NULL;
    PAttribute EaAttribute;
    PAttribute InformationAttribute;
    PAttribute StandardAttribute;
    PStandardInformationEx Standard;
    PUCHAR ExistingData = NULL;
    PUCHAR FinalData = NULL;
    PUCHAR RecordBackup = NULL;
    EAInformationEx ExistingInformation;
    EAInformationEx FinalInformation;
    ULONG BuildEntryCount = 0;
    ULONG EntryCapacity;
    ULONG ExistingCount = 0;
    ULONG ExistingLength = 0;
    ULONG FinalLength = 0;
    BOOLEAN Committed = FALSE;
    BOOLEAN Promote = FALSE;
    NTSTATUS Status;

    Status = ValidateEaUpdates(Updates,
                               UpdateCount);
    if (!NT_SUCCESS(Status) || UpdateCount == 0)
        return Status;
    if (!DiskVolume)
        return STATUS_INVALID_DEVICE_STATE;
    if (DiskVolume->IsReadOnly)
        return STATUS_ACCESS_DENIED;
    if (FindAttributeInRecord(TypeAttributeList,
                              NULL,
                              NULL))
    {
        return STATUS_NOT_IMPLEMENTED;
    }

    Status = GetStandardInformationForUpdate(
        &StandardAttribute,
        &Standard);
    if (!NT_SUCCESS(Status))
        return Status;
    UNREFERENCED_PARAMETER(StandardAttribute);
    Status = LoadExistingExtendedAttributes(
        this,
        &ExistingData,
        &ExistingLength,
        &ExistingInformation,
        &ExistingCount);
    if (!NT_SUCCESS(Status))
        goto Done;

    if (ExistingCount > MAXULONG - UpdateCount)
    {
        Status = STATUS_EA_TOO_LARGE;
        goto Done;
    }
    EntryCapacity = ExistingCount + UpdateCount;
    if ((SIZE_T)EntryCapacity >
        (~(SIZE_T)0) / sizeof(*Entries))
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }

    Entries = new(PagedPool, TAG_NTFS)
        EA_BUILD_ENTRY[EntryCapacity];
    if (!Entries)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }
    RtlZeroMemory(Entries,
                  sizeof(*Entries) * EntryCapacity);

    Status = PopulateEaBuildEntries(
        ExistingData,
        ExistingLength,
        ExistingCount,
        Updates,
        UpdateCount,
        Entries,
        &BuildEntryCount);
    if (!NT_SUCCESS(Status))
        goto Done;

    Status = BuildRawExtendedAttributes(
        Entries,
        BuildEntryCount,
        &FinalData,
        &FinalLength,
        &FinalInformation);
    if (!NT_SUCCESS(Status))
        goto Done;

    if (ExtendedAttributesAreIdentical(
            ExistingData,
            ExistingLength,
            &ExistingInformation,
            FinalData,
            FinalLength,
            &FinalInformation))
    {
        Status = STATUS_SUCCESS;
        goto Done;
    }

    InformationAttribute =
        FindAttributeInRecord(TypeEAInformation,
                              NULL,
                              NULL);
    EaAttribute = FindAttributeInRecord(TypeEA,
                                        NULL,
                                        NULL);
    if (!!InformationAttribute != !!EaAttribute ||
        (!!EaAttribute != (ExistingLength != 0)))
    {
        Status = STATUS_EA_CORRUPT_ERROR;
        goto Done;
    }
    if (EaAttribute)
    {
        if (InformationAttribute->NameLength != 0 ||
            InformationAttribute->Flags != 0 ||
            InformationAttribute->IsNonResident ||
            EaAttribute->NameLength != 0 ||
            EaAttribute->Flags != 0 ||
            GetAttributeOwner(InformationAttribute) !=
                this ||
            GetAttributeOwner(EaAttribute) != this)
        {
            Status = STATUS_NOT_IMPLEMENTED;
            goto Done;
        }
        if (EaAttribute->IsNonResident)
        {
            if (EaAttribute->NonResident.FirstVCN != 0)
            {
                Status = STATUS_NOT_IMPLEMENTED;
                goto Done;
            }
            OldEaRuns = FindNonResidentData(EaAttribute);
            if (!OldEaRuns)
            {
                Status = STATUS_EA_CORRUPT_ERROR;
                goto Done;
            }
        }
    }

    RecordBackup =
        new(PagedPool, TAG_FILE_RECORD)
            UCHAR[RecordBufferSize];
    if (!RecordBackup)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }
    RtlCopyMemory(RecordBackup,
                  Data,
                  RecordBufferSize);

    if (EaAttribute)
    {
        Status = RemoveAttributeRecord(EaAttribute);
        if (!NT_SUCCESS(Status))
            goto Restore;
        InformationAttribute =
            FindAttributeInRecord(TypeEAInformation,
                                  NULL,
                                  NULL);
        if (!InformationAttribute)
        {
            Status = STATUS_EA_CORRUPT_ERROR;
            goto Restore;
        }
        Status =
            RemoveAttributeRecord(InformationAttribute);
        if (!NT_SUCCESS(Status))
            goto Restore;
    }

    if (FinalLength != 0)
    {
        Status = InsertResidentAttribute(
            TypeEAInformation,
            NULL,
            &InformationAttribute);
        if (!NT_SUCCESS(Status))
            goto Restore;
        Status = ReplaceResidentData(
            InformationAttribute,
            reinterpret_cast<const UCHAR*>(
                &FinalInformation),
            sizeof(FinalInformation));
        if (!NT_SUCCESS(Status))
            goto Restore;

        Status = InsertResidentAttribute(TypeEA,
                                         NULL,
                                         &EaAttribute);
        if (!NT_SUCCESS(Status))
            goto Restore;
        Status = ReplaceResidentData(EaAttribute,
                                     FinalData,
                                     FinalLength);
        if (Status == STATUS_BUFFER_TOO_SMALL)
        {
            Promote = TRUE;
            Status = STATUS_SUCCESS;
        }
        else if (!NT_SUCCESS(Status))
        {
            goto Restore;
        }
    }

    Status = GetStandardInformationForUpdate(
        &StandardAttribute,
        &Standard);
    if (!NT_SUCCESS(Status))
        goto Restore;
    UNREFERENCED_PARAMETER(StandardAttribute);
    Standard->FilePermissions |= FILE_PERM_ARCHIVE;

    Status = SynchronizeFileNameInformation(
        NTFS_FILE_NAME_UPDATE_EA_SIZE |
            NTFS_FILE_NAME_UPDATE_ARCHIVE,
        0,
        0,
        FinalInformation.PackedEASize,
        0,
        0);
    if (!NT_SUCCESS(Status))
        goto Restore;

    if (Promote)
    {
        Status = PromoteResidentData(
            EaAttribute,
            FinalData,
            FinalLength,
            0,
            FinalLength,
            FinalLength);
        if (!NT_SUCCESS(Status))
            goto Restore;
        Committed = TRUE;
    }
    else
    {
        Status = PrepareAutomaticTimestamps(
            NTFS_BASIC_INFO_CHANGE_TIME,
            NULL);
        if (!NT_SUCCESS(Status))
            goto Restore;
        Status = DiskVolume->MFT->
            WriteFileRecordToMFT(this);
        if (!NT_SUCCESS(Status))
            goto Restore;
        Committed = TRUE;
    }

    if (OldEaRuns)
        Status = DiskVolume->ReleaseClusters(OldEaRuns);
    goto Done;

Restore:
    if (!Committed)
    {
        RtlCopyMemory(Data,
                      RecordBackup,
                      RecordBufferSize);
        Header =
            reinterpret_cast<PFileRecordHeader>(Data);
        ClearDataRunCache();
    }

Done:
    FreeDataRun(OldEaRuns);
    delete[] RecordBackup;
    delete[] FinalData;
    delete[] Entries;
    delete[] ExistingData;
    return Status;
}
