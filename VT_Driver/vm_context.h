#pragma once

#ifndef _VM_CONTEXT_H
#define _VM_CONTEXT_H

EXTERN_C
{
	void vmm_entrypoint();
	void vmx_save_state();
	void vmx_restore_state();
}

#endif // !_VM_CONTEXT_H
