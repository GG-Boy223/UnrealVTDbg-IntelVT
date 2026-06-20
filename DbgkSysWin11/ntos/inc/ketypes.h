#pragma once

#ifndef _KETYPES_H
#define _KETYPES_H

//
// Define the size of FP registers in the FXSAVE format
//
#define SIZE_OF_FX_REGISTERS        128

//
//  Define the size of the 80387 save area, which is in the context frame.
//

#define SIZE_OF_80387_REGISTERS      80

typedef ULONG MM_PROTECTION_MASK;

#endif // !_KETYPES_H
