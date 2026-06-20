#pragma once

#ifndef _DEBUG_BREAK_H
#define _DEBUG_BREAK_H

//设置硬件断点
void SetHardwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp);
//移除硬件断点
void RemoveHardwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp);

//设置软件断点
void SetSoftwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp);
//移除软件断点
void RemoveSoftwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp);
//读int3断点
void ReadSoftwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp);



#endif // !_DEBUG_BREAK_H