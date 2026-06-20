#pragma once

#ifndef _GLOBALS_MACROS_H
#define _GLOBALS_MACROS_H

#define GET_PFN(_VAR_) (_VAR_ >> PAGE_SHIFT) // 从物理地址得到页帧号（PFN）

#define MASK_GET_HIGHER_32BITS(_ARG_)(_ARG_ & 0xffffffff00000000) // 获取 64 位值的高 32 位（掩码方式）
#define MASK_GET_LOWER_32BITS(_ARG_)(_ARG_ & 0xffffffff) // 获取 64 位值的低 32 位（掩码方式）
#define MASK_GET_LOWER_16BITS(_ARG_)(_ARG_ & 0xffff) // 获取 64 位值的低 16 位（掩码方式）
#define MASK_GET_LOWER_8BITS(_ARG_)(_ARG_ & 0xff) // 获取 64 位值的低 8 位（掩码方式）

#define MASK_EPT_PML1_OFFSET(_VAR_) ((unsigned __int64)_VAR_ & 0xFFFULL)            // 计算 EPT PML1（PTE）内偏移
#define MASK_EPT_PML1_INDEX(_VAR_) ((_VAR_ & 0x1FF000ULL) >> 12)                    // 计算 EPT PML1（PTE）索引
#define MASK_EPT_PML2_INDEX(_VAR_) ((_VAR_ & 0x3FE00000ULL) >> 21)                  // 计算 EPT PML2（PDE）索引
#define MASK_EPT_PML3_INDEX(_VAR_) ((_VAR_ & 0x7FC0000000ULL) >> 30)                // 计算 EPT PML3（PDPTE）索引
#define MASK_EPT_PML4_INDEX(_VAR_) ((_VAR_ & 0xFF8000000000ULL) >> 39)              // 计算 EPT PML4（PML4E）索引

#endif // !_GLOBALS_MACROS_H
