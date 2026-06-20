#pragma once

#ifndef _EXCEPTION_H
#define _EXCEPTION_H

union __exception_bitmap
{
    unsigned __int32 all;
    struct
    {
        unsigned __int32 divide_error : 1;  //0
        unsigned __int32 debug : 1;  //1
        unsigned __int32 nmi_interrupt : 1;  //2
        unsigned __int32 breakpoint : 1;  //3
        unsigned __int32 overflow : 1;  //4
        unsigned __int32 bound : 1;  //5
        unsigned __int32 invalid_opcode : 1;  //6
        unsigned __int32 device_not_available : 1;  //7
        unsigned __int32 double_fault : 1;  //8
        unsigned __int32 coprocessor_segment_overrun : 1;  //9
        unsigned __int32 invalid_tss : 1;  //10
        unsigned __int32 segment_not_present : 1;  //11
        unsigned __int32 stack_segment_fault : 1;  //12
        unsigned __int32 general_protection : 1;  //13
        unsigned __int32 page_fault : 1;  //14
        unsigned __int32 reserved_0 : 1;  //15
        unsigned __int32 floating_point_error : 1;  //16
        unsigned __int32 alignment_check : 1;  //17
        unsigned __int32 machine_check : 1;  //18
        unsigned __int32 simd_floating_point_exception : 1;  //19
        unsigned __int32 virtualization_exception : 1;  //20
        unsigned __int32 control_protection_exception : 1;  //21
    };
};

#endif // !_EXCEPTION_H
