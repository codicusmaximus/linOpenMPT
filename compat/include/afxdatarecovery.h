// linOpenMPT compatibility layer: stand-in for MFC <afxdatarecovery.h>
// (Restart Manager support: autosave and reopening documents after a
// crash or update). The platform layer implements it in step 3.
#pragma once
#include "afxtempl.h"

class CDataRecoveryHandler : public CObject
{
public:
	CDataRecoveryHandler(DWORD restartManagerSupportFlags, int autosaveInterval)
		: m_dwRestartManagerSupportFlags(restartManagerSupportFlags), m_nAutosaveInterval(autosaveInterval) { }
	~CDataRecoveryHandler() override = default;

	virtual BOOL Initialize();
	virtual BOOL SaveOpenDocumentList();
	virtual BOOL ReadOpenDocumentList();
	virtual BOOL ReopenPreviousDocuments();
	virtual BOOL AutosaveAllDocumentInfo();
	virtual BOOL RemoveDocumentInfo(CDocument *document);
	virtual CString GetRestartIdentifier() const { return m_strRestartIdentifier; }
	virtual void SetRestartIdentifier(const CString &id) { m_strRestartIdentifier = id; }
	virtual int GetAutosaveInterval() const { return m_nAutosaveInterval; }

protected:
	DWORD m_dwRestartManagerSupportFlags = 0;
	int m_nAutosaveInterval = 0;
	CString m_strRestartIdentifier;
	CMap<CString, LPCTSTR, CString, LPCTSTR> m_mapDocNameToAutosaveName;
};
