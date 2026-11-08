// linOpenMPT compatibility layer: stand-in for Windows <Msctf.h> (Text
// Services Framework). OpenMPT uses it to follow keyboard layout changes;
// on Linux the Qt UI handles that, so these are inert interface skeletons.
#pragma once
#include "afx.h"
#include "linopenmpt/win32_shell.h"

#ifndef __RPC__out
#define __RPC__out
#endif
#define TF_INVALID_COOKIE (0xffffffff)

struct ITfLanguageProfileNotifySink : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnLanguageChange(LANGID langid, BOOL *pfAccept) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnLanguageChanged() = 0;
};
struct ITfInputProcessorProfiles : public IUnknown { };
struct ITfSource : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE AdviseSink(REFIID riid, IUnknown *punk, DWORD *cookie) = 0;
	virtual HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD cookie) = 0;
};
// No COM server provides these on Linux, so CoCreateInstance() fails and
// the tracker's language profile sink stays unused.
extern const CLSID CLSID_TF_InputProcessorProfiles;
extern const IID IID_ITfInputProcessorProfiles;
extern const IID IID_ITfSource;
extern const IID IID_ITfLanguageProfileNotifySink;
