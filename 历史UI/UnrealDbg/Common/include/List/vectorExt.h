#pragma once

#ifndef _VECTOREXT_H

template <class _Ty>
class vectorExt : public std::vector<_Ty>
{
private:
	CRITICAL_SECTION FLock;

public:
	vectorExt()
	{
		InitializeCriticalSection(&FLock);
	}

	~vectorExt()
	{
		DeleteCriticalSection(&FLock);
	}

	void Lock()
	{
		EnterCriticalSection(&FLock);
	}

	void UnLock()
	{
		LeaveCriticalSection(&FLock);
	}
};

#endif // !_VECTOREXT_H

