#pragma once

#ifndef _MY_LIST_H
#define _MY_LIST_H


typedef struct _MY_LIST
{
	LIST_ENTRY ListHead;
	ULONG Count; // 用户自定义数据
} MY_LIST;

VOID InitFunctionEventList();
// 初始化函数事件列表
VOID InitializeList(MY_LIST* list, PFAST_MUTEX FastMutex);
VOID InsertNode(MY_LIST* list, LIST_ENTRY* entry);
VOID RemoveNode(MY_LIST* list, LIST_ENTRY* entry);
ULONG GetListCount(MY_LIST* list);


class CMyList
{
private:
	DWORD m_FCount;  // 记录元素个数
	DWORD m_FCapacity;  // 最大容量
	FAST_MUTEX m_FLock;
	PVOID* m_FList;  // 指向元素数组的指针

public:
	CMyList()
	{

	}
	CMyList(DWORD MaxCount)
	{
		m_FCount = 0;
		m_FCapacity = MaxCount;
		m_FList = new PVOID[MaxCount];
		RtlZeroMemory(m_FList, MaxCount * sizeof(PVOID));
		ExInitializeFastMutex(&m_FLock);
	}

	~CMyList()
	{
		delete[] m_FList;
	}

	VOID Lock()
	{
		ExAcquireFastMutex(&m_FLock);
	}

	VOID UnLock()
	{
		ExReleaseFastMutex(&m_FLock);
	}

	DWORD Count()
	{
		return m_FCount;
	}

	// 取元素
	PVOID Get(DWORD dwIndex)
	{
		if (dwIndex >= m_FCount)
		{
			//// 检查前面数组元素是否已经初始化完成
			//if (!IsDebugger(PsGetCurrentProcess()))
			//{
			//    // 尝试获取窗口对象
			//    PWND pwnd = Sys_ValidateHwnd(hwnd);
			//    if (pwnd)
			//    {
			//        // 获取线程进程
			//        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
			//    }
			//}
			//NtUserQueryWindow(hwnd, WindowInfo);
			return NULL;
		}
		else
		{
			return m_FList[dwIndex];
		}
	}

	// 删元素
	VOID Delete(DWORD dwIndex)
	{
		if (dwIndex < m_FCount)
		{
			--m_FCount;
			RtlCopyMemory(&m_FList[dwIndex], &m_FList[dwIndex + 1], (m_FCount - dwIndex) * sizeof(PVOID));
		}
	}

	// 增加元素
	DWORD Add(PVOID Item)
	{
		if (m_FCount < m_FCapacity)
		{
			m_FList[m_FCount++] = Item;
		}
		else
		{
			// 超过元素最大限制通过int3触发断点
			DbgPrint("CMyList capacity exceeded: count=%lu capacity=%lu\n", m_FCount, m_FCapacity);
		}
		return m_FCount;
	}
};


#endif // !_MY_LIST_H