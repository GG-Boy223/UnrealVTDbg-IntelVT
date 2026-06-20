#pragma once

#ifndef _VMM_H
#define _VMM_H

/// <summary>
/// Initialize and launch vmm
/// </summary>
/// <returns> status </returns>
bool vmm_init();

/// <summary>
/// Deallocate all structures
/// </summary>
void free_vmm_context();

#endif // !_VMM_H
