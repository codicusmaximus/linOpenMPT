// linOpenMPT compatibility layer: stand-in for Windows <Msctf.h> (Text
// Services Framework). OpenMPT uses it to follow keyboard layout changes;
// on Linux the Qt UI handles that, so these are inert interface skeletons.
#pragma once
#include "afx.h"

#ifndef __RPC__out
#define __RPC__out
#endif
#define TF_INVALID_COOKIE (0xffffffff)

struct IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) = 0;
	virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
	virtual ULONG STDMETHODCALLTYPE Release() = 0;
	virtual ~IUnknown() = default;
};
struct ITfLanguageProfileNotifySink : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnLanguageChange(LANGID langid, BOOL *pfAccept) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnLanguageChanged() = 0;
};
struct ITfInputProcessorProfiles : public IUnknown { };
struct ITfSource : public IUnknown { };
