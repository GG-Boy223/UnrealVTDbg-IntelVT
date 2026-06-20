#pragma once

#ifndef _ASM_VAR_H
#define _ASM_VAR_H

extern "C"
{
	extern unsigned __int64 DbgkCreateThread_jcc_instruction;
	extern unsigned __int64 PspExitThread_jcc_instruction;
	void Asm_DbgkCreateThread_CMP_Debugport_01();
	void Asm_PspExitThread_CMP_Debugport_01();
}


#endif // !_ASM_VAR_H
