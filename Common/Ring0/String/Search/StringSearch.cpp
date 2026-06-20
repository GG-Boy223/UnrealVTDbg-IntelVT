#include <ntifs.h>
#include "../StringHandler.h"

BOOLEAN RtlUnicodeStringContains(PUNICODE_STRING Str, PUNICODE_STRING SubStr, BOOLEAN CaseInsensitive)
{
    if (Str == NULL || SubStr == NULL || Str->Length < SubStr->Length)
    {
        return FALSE;
    }

    CONST USHORT NumCharsDiff = (Str->Length - SubStr->Length) / sizeof(WCHAR);
    UNICODE_STRING Slice = *Str;
    Slice.Length = SubStr->Length;

    for (USHORT i = 0; i <= NumCharsDiff; ++i, ++Slice.Buffer, Slice.MaximumLength -= sizeof(WCHAR))
    {
        if (RtlEqualUnicodeString(&Slice, SubStr, CaseInsensitive))
        {
            return TRUE;
        }
    }
    return FALSE;
}
