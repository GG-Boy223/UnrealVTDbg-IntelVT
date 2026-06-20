#include "Driver.h"
#include "poolmanager.h"
#include "Globals.h"
#include "mtrr.h"
#include "EPT.h"
#include "vmexit_handler.h"
#include "idt.h"

namespace hv
{
	// create an interrupt gate that points to the supplied interrupt handler
	// 创建一个指向提供的中断处理程序的中断门
	static segment_descriptor_interrupt_gate_64 create_interrupt_gate(void* const handler) {
		segment_descriptor_interrupt_gate_64 gate;

		gate.interrupt_stack_table = 0;
		gate.segment_selector = host_cs_selector.flags;
		gate.must_be_zero_0 = 0;
		gate.type = SEGMENT_DESCRIPTOR_TYPE_INTERRUPT_GATE;
		gate.must_be_zero_1 = 0;
		gate.descriptor_privilege_level = 0;
		gate.present = 1;
		gate.reserved = 0;

		auto const offset = reinterpret_cast<uint64_t>(handler);
		gate.offset_low = (offset >> 0) & 0xFFFF;
		gate.offset_middle = (offset >> 16) & 0xFFFF;
		gate.offset_high = (offset >> 32) & 0xFFFFFFFF;

		return gate;
	}

	// initialize the host IDT and populate every descriptor
	// 初始化主机IDT并填充每个描述符
	void prepare_host_idt(segment_descriptor_interrupt_gate_64* const idt) {
		memset(idt, 0, HOST_IDT_DESCRIPTOR_COUNT * sizeof(idt[0]));
		idt[0] = create_interrupt_gate(interrupt_handler_0);
		idt[1] = create_interrupt_gate(interrupt_handler_1);
		idt[2] = create_interrupt_gate(interrupt_handler_2);
		idt[3] = create_interrupt_gate(interrupt_handler_3);
		idt[4] = create_interrupt_gate(interrupt_handler_4);
		idt[5] = create_interrupt_gate(interrupt_handler_5);
		idt[6] = create_interrupt_gate(interrupt_handler_6);
		idt[7] = create_interrupt_gate(interrupt_handler_7);
		idt[8] = create_interrupt_gate(interrupt_handler_8);
		idt[10] = create_interrupt_gate(interrupt_handler_10);
		idt[11] = create_interrupt_gate(interrupt_handler_11);
		idt[12] = create_interrupt_gate(interrupt_handler_12);
		idt[13] = create_interrupt_gate(interrupt_handler_13);
		idt[14] = create_interrupt_gate(interrupt_handler_14);
		idt[16] = create_interrupt_gate(interrupt_handler_16);
		idt[17] = create_interrupt_gate(interrupt_handler_17);
		idt[18] = create_interrupt_gate(interrupt_handler_18);
		idt[19] = create_interrupt_gate(interrupt_handler_19);
		idt[20] = create_interrupt_gate(interrupt_handler_20);
		idt[30] = create_interrupt_gate(interrupt_handler_30);
	}

	// called for every host interrupt
	EXTERN_C
	void handle_host_interrupt(trap_frame* const frame) {
		switch (frame->vector) 
		{			
		case nmi:  // host NMIs
		{
			//只有在“NMI exiting”以及“virtual-NMIs”都为 1 时，“NMI-window exiting”才能被置位。
			auto ctrl = read_ctrl_proc_based();
			ctrl.nmi_window_exiting = 1;
			write_ctrl_proc_based(ctrl);

			auto const cpu = reinterpret_cast<__vcpu*>(_readfsbase_u64());
			++cpu->queued_nmis;
			sLog("\n");
			break;
		}				
		default: {  // host exceptions
			// no registered exception handler
			if (!frame->r10 || !frame->r11) {
				//HV_LOG_ERROR("Unhandled exception. RIP=hv.sys+%p. Vector=%u.",
				//	frame->rip - reinterpret_cast<UINT64>(&__ImageBase), frame->vector);

				// ensure a triple-fault
				segment_descriptor_register_64 idtr;
				idtr.base_address = frame->rsp;
				idtr.limit = 0xFFF;
				__lidt(&idtr);

				break;
			}

			//HV_LOG_HOST_EXCEPTION("Handling host exception. RIP=hv.sys+%p. Vector=%u",
			//frame->rip - reinterpret_cast<UINT64>(&__ImageBase), frame->vector);

			// jump to the exception handler
			frame->rip = frame->r10;

			auto const e = reinterpret_cast<host_exception_info*>(frame->r11);

			e->exception_occurred = true;
			e->vector = frame->vector;
			e->error = frame->error;

			// slightly helps prevent infinite exceptions
			frame->r10 = 0;
			frame->r11 = 0;
		}
		}
	}
}