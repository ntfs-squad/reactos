/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"
#define LONGLONG_SIGN_EXTEND(Number, Bytes) \
(Number << ((sizeof(LONGLONG) - Bytes) * 8)) >> ((sizeof(LONGLONG) - Bytes) * 8)

/* Find Attribute Functions */
PAttribute
FileRecord::GetAttribute(_In_     AttributeType Type,
                         _In_opt_ PWSTR Name)
{
    ULONG DataPtr;
    PAttribute TestAttr;
    UINT NameLength = 0;

    // Validate header before using it
    if (!Header || !Data)
        return NULL;

    // Basic sanity: ensure pointers exist; detailed signature checks happen at load time

    if (Name)
        NameLength = wcslen(Name) * sizeof(WCHAR);

    // Progress data pointer to attribute section.
    DataPtr = Header->AttributeOffset;

    // Ensure AttributeOffset is inside the record bounds
    if (Header->AttributeOffset < sizeof(FileRecordHeader) ||
        Header->AttributeOffset >= Header->ActualSize ||
        Header->ActualSize > Header->AllocatedSize)
    {
        return NULL;
    }

    while (DataPtr + 0x10 /* minimum fixed header */ <= Header->ActualSize)
    {
        // Test current attribute
        TestAttr = (PAttribute)(&Data[DataPtr]);

        // Guard against corrupted attributes and end marker
        if (TestAttr->AttributeType == TypeAttributeEndMarker)
            return NULL;

        // Validate attribute length (resident: >= 0x18, nonresident: >= 0x40)
        const ULONG minHeader = TestAttr->IsNonResident ? 0x40 : 0x18;
        if (TestAttr->Length < minHeader ||
            (TestAttr->Length & 7) != 0 ||
            DataPtr + TestAttr->Length > Header->ActualSize)
        {
            return NULL;
        }

        if (TestAttr->AttributeType == Type)
        {
            // If no name is specified, return the first attribute with the target type.
            if (!Name)
            {
                return (PAttribute)&Data[DataPtr];
            }

            // Validate name fields before comparing
            if (TestAttr->NameLength &&
                TestAttr->NameOffset >= minHeader &&
                TestAttr->NameOffset + (TestAttr->NameLength * sizeof(WCHAR)) <= TestAttr->Length)
            {
                if ((TestAttr->NameLength) * sizeof(WCHAR) == NameLength &&
                    RtlCompareMemory(GetNamePointer(TestAttr),
                                     Name,
                                     NameLength) == NameLength)
                {
                    // We found the attribute!
                    return (PAttribute)&Data[DataPtr];
                }
            }

            // This one isn't it. Try again.
        }

        else if (!TestAttr->Length ||
                 TestAttr->AttributeType > (ULONG)Type)
        {
            /* Attributes are stored in ascending order according to its attribute type.
             * If we passed the attribute type, it's not going to show up. Fail early.
             *
             * TODO: Search $ATTRIBUTE_LIST (0x20) if we can't find it.
             */
            return NULL;
        }

        DataPtr += TestAttr->Length;
    }

    // We went through the whole record and didn't find it.
    return NULL;
}

PDataRun
FileRecord::FindNonResidentData(_In_ PAttribute DataAttr)
{

    /* Algorithm:
     * 1. Read Data run. (Stored in $DATA attribute)
     * 2. Parse it.
     *   - First byte describes the length of the offset and length`.
     *   - Next section is the length (in clusters).
     *   - Next section is the offset^ (in clusters).
     *   - Next byte is the next data run (if it's fragmented).
     *   - List of data runs is terminated with 0x00.
     * 3. Return head of linked list.
     *
     * `: First nibble is the length. Second nibble is the offset.
     *
     * ^: From 0 for the first, from the previous offset for the rest.
     *    Can be negative.
     */

    PUCHAR DataRunPtr;
    PDataRun Head, Temp;
    UINT8 LengthSize, OffsetSize;
    ULONGLONG PreviousLCN = 0;
    LONGLONG TestOffset = 0;
    ULONGLONG AllocatedSize = DataAttr->NonResident.AllocatedSize;
    ULONGLONG ReadSize = 0;

    if (!DataAttr || !DataAttr->IsNonResident)
        return NULL;

    // Get pointer to data run.
    DataRunPtr = (PUCHAR)DataAttr + DataAttr->NonResident.DataRunsOffset;

    // Populate Head.
    Head = new(PagedPool, TAG_DATA_RUN) DataRun();
    Head->NextRun = NULL;

    // Length size is LSB, Offset size is MSB.
    LengthSize = *DataRunPtr & 0xF;
    OffsetSize = *DataRunPtr >> 4;

    // Copy length and LCN into linked list head.
    RtlCopyMemory(&Head->Length,
                  DataRunPtr + 1,
                  LengthSize);

    RtlCopyMemory(&Head->LCN,
                  DataRunPtr + 1 + LengthSize,
                  OffsetSize);

    // Populate children data runs for head, if available.
    Temp = Head;
    PreviousLCN = Temp->LCN;
    ReadSize += (Head->Length) * Volume->SectorsPerCluster * Volume->BytesPerSector;
    DataRunPtr += 1 + LengthSize + OffsetSize;

    while (*DataRunPtr != '\0')
    {
        // Initialize next item in linked list.
        Temp->NextRun = new(PagedPool, TAG_DATA_RUN) DataRun();
        Temp = Temp->NextRun;

        // Get length and offset sizes for current data run.
        LengthSize = *DataRunPtr & 0xF;
        OffsetSize = *DataRunPtr >> 4;

        // We don't yet handle offset sizes larger than 8 bytes.
        ASSERT(OffsetSize <= sizeof(LONGLONG));
        ASSERT(OffsetSize != 0);

        // Copy length and LCN into child data run.
        RtlCopyMemory(&Temp->Length,
                      DataRunPtr + 1,
                      LengthSize);

        ReadSize += (Temp->Length) * Volume->SectorsPerCluster * Volume->BytesPerSector;

        /* Note: Child LCN's are relative to previous offset. They can be negative.
         * So the real LCN = Previous LCN + Current LCN.
         */
        TestOffset = 0;
        RtlCopyMemory(&TestOffset,
                      DataRunPtr + 1 + LengthSize,
                      OffsetSize);

        // Sign extend the LCN offset if needed.
        TestOffset = LONGLONG_SIGN_EXTEND(TestOffset, OffsetSize);

        // Assign LCN for this data run
        Temp->LCN = PreviousLCN + TestOffset;

        // Move data run pointer to next item.
        DataRunPtr += OffsetSize + LengthSize + 1;

        // Update Previous Offset
        PreviousLCN = Temp->LCN;
    }

    ASSERT(ReadSize == AllocatedSize);

    // The caller is responsible for freeing the linked list.
    return Head;
}

PDataRun
FileRecord::FindNonResidentData(_In_     AttributeType Type,
                                _In_opt_ PWSTR Name)
{
    return FindNonResidentData(GetAttribute(Type, Name));
}