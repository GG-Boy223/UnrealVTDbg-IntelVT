#pragma once

#ifndef _CRX_H
#define _CRX_H


enum __cr_access_type
{
    CR_ACCESS_MOV_TO_CR,
    CR_ACCESS_MOV_FROM_CR,
    CR_ACCESS_CLTS,
    CR_ACCESS_LMSW
};

union __cr_access_qualification
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 cr_number : 4;
        unsigned __int64 access_type : 2;
        unsigned __int64 operand_type : 1;
        unsigned __int64 reserved1 : 1;
        unsigned __int64 register_type : 4;
        unsigned __int64 reserved2 : 4;
        unsigned __int64 source_data : 16;
        unsigned __int64 reserved3 : 32;
    };
};

union __cr_fixed
{
    unsigned __int64 all;
    struct
    {
        unsigned long low;
        long high;
    } split;
    struct
    {
        unsigned long low;
        long high;
    } u;
};

union __cr8
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 task_priority_level : 4;
        unsigned __int64 reserved : 59;
    };
};

union __cr0

{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 protection_enable : 1;
        unsigned __int64 monitor_coprocessor : 1;
        unsigned __int64 emulate_fpu : 1;
        unsigned __int64 task_switched : 1;
        unsigned __int64 extension_type : 1;
        unsigned __int64 numeric_error : 1;
        unsigned __int64 reserved_1 : 10;
        unsigned __int64 write_protect : 1;
        unsigned __int64 reserved_2 : 1;
        unsigned __int64 alignment_mask : 1;
        unsigned __int64 reserved_3 : 10;
        unsigned __int64 not_write_through : 1;
        unsigned __int64 cache_disable : 1;
        unsigned __int64 paging_enable : 1;
        unsigned __int64 reserved_4 : 32;
    };
};

union __cr2
{
    unsigned __int64 linear_address;
};

union __cr3
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 pcid : 12;
        unsigned __int64 page_frame_number : 36;
        unsigned __int64 reserved_1 : 12;
        unsigned __int64 reserved_2 : 3;
        unsigned __int64 pcid_invalidate : 1;
    };
};

union __cr4
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 virtual_mode_extensions : 1;  //bit 0
        unsigned __int64 protected_mode_virtual_interrupts : 1;  //bit 1
        unsigned __int64 timestamp_disable : 1; //bit 2
        unsigned __int64 debugging_extensions : 1;  //bit 3
        unsigned __int64 page_size_extensions : 1;  //bit 4
        unsigned __int64 physical_address_extension : 1;  //bit 5
        unsigned __int64 machine_check_enable : 1; //bit 6
        unsigned __int64 page_global_enable : 1; //bit 7
        unsigned __int64 performance_monitoring_counter_enable : 1; //bit 8
        unsigned __int64 os_fxsave_fxrstor_support : 1;  //bit 9
        unsigned __int64 os_xmm_exception_support : 1;  //bit 10
        unsigned __int64 usermode_instruction_prevention : 1; //bit 11
        unsigned __int64 linear_addresses_57_bit : 1; //bit 12
        unsigned __int64 vmx_enable : 1;  //bit 13
        unsigned __int64 smx_enable : 1;  //bit 14
        unsigned __int64 reserved_1 : 1;  //bit 15
        unsigned __int64 fsgsbase_enable : 1;  //bit 16
        unsigned __int64 pcid_enable : 1;  //bit 17
        unsigned __int64 os_xsave : 1;  //bit 18
        unsigned __int64 key_locker_enable : 1;  //bit 19
        unsigned __int64 smep_enable : 1;  //bit 20
        unsigned __int64 smap_enable : 1;  //bit 21
        unsigned __int64 protection_key_enable : 1;  //bit 22
    };
};

union __xcr0
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 x87 : 1;  //bit 0
        unsigned __int64 sse : 1;  //bit 1
        unsigned __int64 avx : 1;  //bit 2
        unsigned __int64 bndreg : 1;  //bit 3
        unsigned __int64 bndcsr : 1;  //bit 4
        unsigned __int64 opmask : 1;  //bit 5
        unsigned __int64 zmm_hi256 : 1;  //bit 6
        unsigned __int64 hi16_zmm : 1;  //bit 7
        unsigned __int64 reserved1 : 1;  //bit 8
        unsigned __int64 pkru : 1;  //bit 9
        unsigned __int64 reserved2 : 1;  //bit 10
        unsigned __int64 cet_user_state : 1;  //bit 11
        unsigned __int64 cet_supervisor_state : 1;  //bit 12
        unsigned __int64 xaad : 1;  //bit 13
        unsigned __int64 reserved3 : 3; //bit 14
        unsigned __int64 TILECFG : 1;  //bit 17
        unsigned __int64 TILEDATA : 1;  //bit 18
        unsigned __int64 reserved4 : 45;
    };
};

#endif // !_CRX_H
