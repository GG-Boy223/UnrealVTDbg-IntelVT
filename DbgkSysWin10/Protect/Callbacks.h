#pragma once

#ifndef _CALLBACKS_H
#define _CALLBACKS_H

//注意此保护方案无法拦截FindWindow
OB_PREOP_CALLBACK_STATUS preProcessCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION pOperationInformation);

//设置进程回调
VOID SetProcessCallbacks(IN PDRIVER_OBJECT pDriver_Object);

//卸载进程回调
VOID UnProcessCallbacks();


#endif // !_CALLBACKS_H
