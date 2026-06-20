#pragma once

#ifndef _SEGMENT_H
#define _SEGMENT_H

enum __segment_registers
{
    ES = 0,
    CS,
    SS,
    DS,
    FS,
    GS,
    LDTR,
    TR
};

union __segment_access_rights
{
    struct
    {
        unsigned __int32 type : 4;
        unsigned __int32 descriptor_type : 1;
        unsigned __int32 dpl : 2;
        unsigned __int32 present : 1;
        unsigned __int32 reserved0 : 4;
        unsigned __int32 available : 1;
        unsigned __int32 long_mode : 1;
        unsigned __int32 default_big : 1;
        unsigned __int32 granularity : 1;
        unsigned __int32 unusable : 1;
        unsigned __int32 reserved1 : 15;
    };

    unsigned __int32 all;
};

struct __segment_descriptor
{
    unsigned __int16 limit_low;
    unsigned __int16 base_low;
    union
    {
        struct
        {
            unsigned __int32 base_middle : 8;
            unsigned __int32 type : 4;
            unsigned __int32 descriptor_type : 1;
            unsigned __int32 dpl : 2;
            unsigned __int32 present : 1;
            unsigned __int32 segment_limit_high : 4;
            unsigned __int32 system : 1;
            unsigned __int32 long_mode : 1;
            unsigned __int32 default_big : 1;
            unsigned __int32 granularity : 1;
            unsigned __int32 base_high : 8;
        };
    };

    unsigned __int32 base_upper;
    unsigned __int32 reserved;
};

union __segment_selector
{
    unsigned short all;
    struct
    {
        unsigned short rpl : 2;
        unsigned short ti : 1;
        unsigned short index : 13;
    };
};

#endif // !_SEGMENT_H
