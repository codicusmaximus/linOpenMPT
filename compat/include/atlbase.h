// linOpenMPT compatibility layer: the part of ATL (<atlbase.h>) that
// OpenMPT uses: CComPtr, and the memory streams GDI+ loads images from.
#pragma once
#include "linopenmpt/win32_shell.h"

// A read-only in-memory byte stream (COM IStream).
struct IStream : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Read(void *data, ULONG size, ULONG *read) = 0;
	// The bytes and their size, so that image decoders can read them at once.
	virtual const BYTE *Data() const = 0;
	virtual ULONG Size() const = 0;
};
// Creates a stream over a GMEM_MOVEABLE block, as on Windows.
HRESULT CreateStreamOnHGlobal(HGLOBAL global, BOOL deleteOnRelease, IStream **stream);
// Copies the data. Returns nullptr on failure.
IStream *SHCreateMemStream(const BYTE *data, UINT size);

template <typename T>
class CComPtr
{
public:
	CComPtr() noexcept = default;
	CComPtr(T *p) noexcept : p(p) { if(p) p->AddRef(); }
	CComPtr(const CComPtr &other) noexcept : CComPtr(other.p) { }
	CComPtr(CComPtr &&other) noexcept : p(other.p) { other.p = nullptr; }
	~CComPtr() { Release(); }
	CComPtr &operator=(const CComPtr &other) noexcept
	{
		if(other.p) other.p->AddRef();
		Release();
		p = other.p;
		return *this;
	}
	CComPtr &operator=(CComPtr &&other) noexcept
	{
		if(this != &other)
		{
			Release();
			p = other.p;
			other.p = nullptr;
		}
		return *this;
	}
	void Attach(T *other) noexcept { Release(); p = other; }
	T *Detach() noexcept { T *r = p; p = nullptr; return r; }
	void Release() noexcept { if(p) { T *old = p; p = nullptr; old->Release(); } }
	operator T *() const noexcept { return p; }
	T *operator->() const noexcept { return p; }
	T **operator&() noexcept { return &p; }
	bool operator!() const noexcept { return p == nullptr; }
	T *p = nullptr;
};
