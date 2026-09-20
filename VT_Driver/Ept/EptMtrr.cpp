#include "../Driver.h"
#include "../poolmanager.h"
#include "../Globals.h"
#include "../mtrr.h"
#include "../EPT.h"
#include "../msr.h"
#include "../AllocateMem.h"
#include "../vmx.h"
#include "../hypervisor_routines.h"
#include "../Vmcs/VmcsApi.h"

namespace ept
{
	/// <summary>
	/// Build mtrr map to track physical memory type
	/// 构建mtrr映射以跟踪物理内存类型(又称缓存类型)
	/// 首先，您需要了解MTRR（Memory Type Range Registers）是什么。
	/// MTRR是一组寄存器，用于定义物理内存地址范围的缓存类型（例如回写缓存WB、直写缓存WT等）。
	/// 然后，您需要获取系统中的MTRR配置信息。可以通过读取和解析系统的MTRR寄存器来获取这些信息。
	/// 具体的方法可能因操作系统和硬件平台而异。
	/// 解析MTRR配置信息并构建MTRR映射。您可以将MTRR配置信息转换为易于理解的格式，例如使用数据结构或表格。
	/// 根据MTRR的设置，将物理内存地址范围映射到相应的缓存类型。
	/// </summary>
	void build_mtrr_map()
	{
		__mtrr_cap_reg mtrr_cap = { 0 };
		__mtrr_physbase_reg current_phys_base = { 0 };
		__mtrr_physmask_reg current_phys_mask = { 0 };
		__mtrr_def_type mtrr_def_type = { 0 };
		__mtrr_range_descriptor* descriptor;

		//
		// 内存类型范围寄存器 (MTRR) 提供了一种关联物理内存类型的机制
		// 用于指定物理内存的缓存类型，提升CPU性能。

		mtrr_cap.all = __readmsr(IA32_MTRRCAP);

		//对于没有被MTRR寄存器涵盖的物理内存区域，使用IA32_MTRR_DEF_TYPE寄存器来指定其默认属性
		mtrr_def_type.all = __readmsr(IA32_MTRR_DEF_TYPE);

		if (mtrr_def_type.mtrr_enabled == false)
		{
			// 不可缓存
			// MTRRs被禁用这意味着所有的物理内存都将被视为UC
			g_vmm_context.mtrr_info.default_memory_type = MEMORY_TYPE_UNCACHEABLE;
			return;
		}

		g_vmm_context.mtrr_info.default_memory_type = mtrr_def_type.memory_type;

		//判断处理器是否支持smrr
		if (mtrr_cap.smrr_support == true)
		{
			current_phys_base.all = __readmsr(IA32_SMRR_PHYSBASE);
			current_phys_mask.all = __readmsr(IA32_SMRR_PHYSMASK);

			// If range is enabled
			// 如果启用范围
			//
			if (current_phys_mask.valid && current_phys_base.type != mtrr_def_type.memory_type)
			{
				descriptor = &g_vmm_context.mtrr_info.memory_range[g_vmm_context.mtrr_info.enabled_memory_ranges++];
				// Calculate base address, physbase is truncated by 12 bits so we have to left shift it by 12
				// 计算基址，physbase 被截断了 12 位，因此我们必须将其左移 12
				//
				descriptor->physcial_base_address = current_phys_base.physbase << PAGE_SHIFT;

				// Index of first bit set to one determines how much do we have to bit shift to get size of range
				// physmask is truncated by 12 bits so we have to left shift it by 12
				// 第一个设置为 1 的位的索引决定了我们需要移位多少位才能得到范围 physmask 的大小，
				// 它被截断了 12 位，所以我们必须将其左移 12 位
				//
				unsigned long bits_in_mask = 0;
				_BitScanForward64(&bits_in_mask, current_phys_mask.physmask << PAGE_SHIFT);

				// Calculate the end of range specified by mtrr
				// 计算 mtrr 指定的范围的结束位置
				//
				descriptor->physcial_end_address = descriptor->physcial_base_address + ((1ULL << bits_in_mask) - 1ULL);
				// Get memory type of range
				// 获取范围的内存类型
				//
				descriptor->memory_type = (unsigned __int8)current_phys_base.type;
				descriptor->fixed_range = false;
			}
		}

		//判断处理器是否支持固定范围MTRR
		//MTRR 机制允许在物理内存中定义多个范围，并定义了一组(MSR)寄存器，用于指定每个范围中包含的内存类型
		//固定内存范围映射为 11 个固定范围寄存器，每个寄存器 64 位。每个寄存器分为 8 个字段，用于指定寄存器控制的每个子范围的内存类型：
		if (mtrr_cap.fixed_range_support == true && mtrr_def_type.fixed_range_mtrr_enabled)
		{
			constexpr auto k64_base = 0x0;
			constexpr auto k64_size = 0x10000; //64KB
			constexpr auto k16_base = 0x80000;
			constexpr auto k16_size = 0x4000; //16KB
			constexpr auto k4_base = 0xC0000;
			constexpr auto k4_size = 0x1000; //4KB

			//寄存器 IA32_MTRR_FIX64K_00000 — 映射 512 KB 地址范围，从 0H 到 7FFFFH。此范围分为8个 64 KB 子范围。
			__mtrr_fixed_range_type k64_types = { __readmsr(IA32_MTRR_FIX64K_00000) };

			//寄存器 IA32_MTRR_FIX4K_C0000 至 IA32_MTRR_FIX4K_F8000 — 映射 8 个 32 KB 地址范围，
			//从 C0000H 到 FFFFFH。此范围分为 64 个 4 KB 子范围，每个寄存器 8 个范围。
			for (unsigned int i = 0; i < 8; i++)
			{
				descriptor = &g_vmm_context.mtrr_info.memory_range[g_vmm_context.mtrr_info.enabled_memory_ranges++];
				descriptor->memory_type = k64_types.types[i];
				descriptor->physcial_base_address = k64_base + (k64_size * i);
				descriptor->physcial_end_address = k64_base + (k64_size * i) + (k64_size - 1);
				descriptor->fixed_range = true;
			}

			//寄存器 IA32_MTRR_FIX16K_80000 和 IA32_MTRR_FIX16K_A0000 — 映射两个 128 KB 地址范围，从 80000H 到 BFFFFH。
			//每个寄存器 8 个范围。
			for (unsigned int i = 0; i < 2; i++)
			{
				__mtrr_fixed_range_type k16_types = { __readmsr(IA32_MTRR_FIX16K_80000 + i) };

				for (unsigned int j = 0; j < 8; j++)
				{
					descriptor = &g_vmm_context.mtrr_info.memory_range[g_vmm_context.mtrr_info.enabled_memory_ranges++];
					descriptor->memory_type = k16_types.types[j];
					descriptor->physcial_base_address = (k16_base + (i * k16_size * 8)) + (k16_size * j);
					descriptor->physcial_end_address = (k16_base + (i * k16_size * 8)) + (k16_size * j) + (k16_size - 1);
					descriptor->fixed_range = true;
				}
			}

			//寄存器 IA32_MTRR_FIX4K_C0000 至 IA32_MTRR_FIX4K_F8000 — 映射 8 个 32 KB 地址范围，
			//从 C0000H 到 FFFFFH。此范围分为 64 个 4 KB 子范围，每个寄存器 8 个范围。
			for (unsigned int i = 0; i < 8; i++)
			{
				__mtrr_fixed_range_type k4_types = { __readmsr(IA32_MTRR_FIX4K_C0000 + i) };

				for (unsigned int j = 0; j < 8; j++)
				{
					descriptor = &g_vmm_context.mtrr_info.memory_range[g_vmm_context.mtrr_info.enabled_memory_ranges++];
					descriptor->memory_type = k4_types.types[j];
					descriptor->physcial_base_address = (k4_base + (i * k4_size * 8)) + (k4_size * j);
					descriptor->physcial_end_address = (k4_base + (i * k4_size * 8)) + (k4_size * j) + (k4_size - 1);
					descriptor->fixed_range = true;
				}
			}
		}


		//Indicates the number of variable ranges
		//implemented on the processor.
		//处理器中可变MTRRs寄存器的数量。
		//Pentium 4、Intel Xeon 和 P6 系列处理器允许软件为 m 个可变大小地址范围指定内存类型，每个范围使用一对 MTRR。
		//支持的范围数 m 在 IA32_MTRRCAP MSR 的位 7:0 中给出
		for (int i = 0; i < mtrr_cap.range_register_number; i++)
		{
			// 每对中的第一个条目（IA32_MTRR_PHYSBASEn）定义范围的基地址和内存类型；
			//
			current_phys_base.all = __readmsr(IA32_MTRR_PHYSBASE0 + (i * 2));
			current_phys_mask.all = __readmsr(IA32_MTRR_PHYSMASK0 + (i * 2));

			//
			// If range is enabled
			// 如果启用范围
			if (current_phys_mask.valid && current_phys_base.type != mtrr_def_type.memory_type)
			{
				descriptor = &g_vmm_context.mtrr_info.memory_range[g_vmm_context.mtrr_info.enabled_memory_ranges++];

				//
				// Calculate base address, physbase is truncated by 12 bits so we have to left shift it by 12
				// 计算基址，physbase 被截断了 12 位，因此我们必须将其左移 12
				//
				descriptor->physcial_base_address = current_phys_base.physbase << PAGE_SHIFT;

				//
				// Index of first bit set to one determines how much do we have to bit shift to get size of range
				// physmask is truncated by 12 bits so we have to left shift it by 12
				// 第一个设置为 1 的位的索引决定了我们需要移位多少位才能得到范围 physmask 的大小，
				// 它被截断了 12 位，所以我们必须将其左移 12 位
				//
				unsigned long bits_in_mask = 0;
				_BitScanForward64(&bits_in_mask, current_phys_mask.physmask << PAGE_SHIFT);

				//
				// Calculate the end of range specified by mtrr
				// 计算 mtrr 指定的范围的结束位置
				//
				descriptor->physcial_end_address = descriptor->physcial_base_address + ((1ULL << bits_in_mask) - 1ULL);

				//
				// Get memory type of range
				// 获取范围的内存类型
				//
				descriptor->memory_type = (unsigned __int8)current_phys_base.type;
				descriptor->fixed_range = false;
			}
		}
	}
}
