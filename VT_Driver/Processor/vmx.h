#pragma once

#ifndef _VMX_H
#define _VMX_H

union __vmx_ept_vpid_cap
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 supports_execute_only : 1;  //0
        unsigned __int64 reserved_0 : 5;  //5:1
        unsigned __int64 support_page_walk_length_4 : 1;  //6
        unsigned __int64 reserved_1 : 1;  //7
        unsigned __int64 ept_support_uc : 1;  //8   uncacheable
        unsigned __int64 reserved_2 : 5;  //13:9
        unsigned __int64 ept_support_wb : 1;  //14  write-back
        unsigned __int64 reserved_3 : 1;  //15
        unsigned __int64 ept_support_2mb_page : 1;  //16
        unsigned __int64 ept_support_1gb_page : 1;  //17
        unsigned __int64 reserved_4 : 2;  //19:18
        unsigned __int64 support_invept_instruction : 1;  //20
        unsigned __int64 support_access_dirty_flags : 1;  //21
        unsigned __int64 reports_advanced_vm_exit_information : 1;  //22
        unsigned __int64 supervisor_shadow_stack_control : 1;  //23
        unsigned __int64 reserved_5 : 1;  //24
        unsigned __int64 support_single_context_invept : 1;  //25
        unsigned __int64 support_all_context_invept : 1;  //26
        unsigned __int64 reserved_6 : 5;  //31:27
        unsigned __int64 support_invvpid_instruction : 1;  //32
        unsigned __int64 reserved_7 : 7;  //39:33
        unsigned __int64 support_individual_address_invvpid : 1;  //40
        unsigned __int64 support_single_context_invvpid : 1;  //41
        unsigned __int64 support_all_context_invvpid : 1;  //42
        unsigned __int64 single_context_retaining_globals_invvpid : 1;  //43
        unsigned __int64 reserved_8 : 4;  //47:44
        unsigned __int64 enumerate_max_hlat_prefix_size : 6;  //53:48
    };
};


#endif // !_VMX_H
