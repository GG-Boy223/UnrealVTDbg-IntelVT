#pragma once

#ifndef _SYMBOLS_H
#define _SYMBOLS_H



//Thanks mambda
//https://bitbucket.org/mambda/pdb-parser/src/master/
struct PDBHeader7
{
	char signature[0x20];
	int page_size;
	int allocation_table_pointer;
	int file_page_count;
	int root_stream_size;
	int reserved;
	int root_stream_page_number_list_number;
};

struct RootStream7
{
	int num_streams;
	int stream_sizes[1]; //num_streams
};

struct GUID_StreamData
{
	int ver;
	int date;
	int age;
	GUID guid;
};

struct PdbInfo
{
	DWORD	Signature;
	GUID	Guid;
	DWORD	Age;
	char	PdbFileName[1];
};


class CStatusCallback :public IBindStatusCallback
{
public:
	CStatusCallback() {}
	~CStatusCallback() {}
	virtual HRESULT STDMETHODCALLTYPE OnStartBinding(
		/* [in] */ DWORD dwReserved,
		/* [in] */ __RPC__in_opt IBinding* pib) {
		// /* [in] */ DWORD dwReserved,
		// /* [in] */ __RPC__in_opt IBinding* pib) {
		return E_NOTIMPL;
	}

	virtual HRESULT STDMETHODCALLTYPE GetPriority(
		/* [out] */ __RPC__out LONG* pnPriority) {
		// /* [out] */ __RPC__out LONG* pnPriority) {
		return E_NOTIMPL;
	}

	virtual HRESULT STDMETHODCALLTYPE OnLowResource(
		/* [in] */ DWORD reserved) {
		// /* [in] */ DWORD reserved) {
		return E_NOTIMPL;
	}

	virtual HRESULT STDMETHODCALLTYPE OnProgress(
		/* [in] */ ULONG ulProgress,
		/* [in] */ ULONG ulProgressMax,
		/* [in] */ ULONG ulStatusCode,
		/* [unique][in] */ __RPC__in_opt LPCWSTR szStatusText) {

		if (!(ulProgressMax | ulProgress))
		{
			return E_NOTIMPL;
		}

		if (ulProgressMax == 0)
		{
			return E_NOTIMPL;
		}

		ULONG download_progress = static_cast<ULONG>(
			(static_cast<double>(ulProgress) / static_cast<double>(ulProgressMax)) * 100.0);


		//char szBuf[MAX_PATH] = { 0 };
		//sprintf(szBuf, "download_progress: %d\n", download_progress);
		//OutputDebugStringA(szBuf);

		SendMessage(g_hwnd, USER_PROGRESS_BAR, download_progress, 0);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnStopBinding(
		/* [in] */ HRESULT hresult,
		/* [unique][in] */ __RPC__in_opt LPCWSTR szError) {
		// /* [in] */ HRESULT hresult,
		// /* [unique][in] */ __RPC__in_opt LPCWSTR szError) {
		return E_NOTIMPL;
	}

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE GetBindInfo(
		/* [out] */ DWORD* grfBINDF,
		/* [unique][out][in] */ BINDINFO* pbindinfo) {
		// /* [out] */ DWORD* grfBINDF,
		// /* [unique][out][in] */ BINDINFO* pbindinfo) {
		return E_NOTIMPL;
	}

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE OnDataAvailable(
		/* [in] */ DWORD grfBSCF,
		/* [in] */ DWORD dwSize,
		/* [in] */ FORMATETC* pformatetc,
		/* [in] */ STGMEDIUM* pstgmed) {
		// /* [in] */ DWORD grfBSCF,
		// /* [in] */ DWORD dwSize,
		// /* [in] */ FORMATETC* pformatetc,
		// /* [in] */ STGMEDIUM* pstgmed) {
		return E_NOTIMPL;
	}

	virtual HRESULT STDMETHODCALLTYPE OnObjectAvailable(
		/* [in] */ __RPC__in REFIID riid,
		/* [iid_is][in] */ __RPC__in_opt IUnknown* punk) {
		// /* [in] */ __RPC__in REFIID riid,
		// /* [iid_is][in] */ __RPC__in_opt IUnknown* punk) {
		return E_NOTIMPL;
	}

	STDMETHOD_(ULONG, AddRef)() { return 0; }
	STDMETHOD_(ULONG, Release)() { return 0; }
	STDMETHOD(QueryInterface)(REFIID riid, void __RPC_FAR* __RPC_FAR* ppvObject) { return S_OK; }
};

bool FileExists(const std::wstring& path);
bool DownloadSymbol_internal(const std::wstring& szModulePath, const std::wstring& path, std::wstring* pdb_path_out, bool Redownload);
bool GetPePdbSig(const std::wstring& pePath, GUID* guid, DWORD* age, DWORD* ts);
bool VerifyExistingPdb2(const GUID& guid, DWORD age);

HRESULT OnProgress(
	unsigned long ulProgress,
	unsigned long ulProgressMax,
	unsigned long ulStatusCode,
	LPCWSTR       szStatusText
);


#endif // !_SYMBOLS_H
