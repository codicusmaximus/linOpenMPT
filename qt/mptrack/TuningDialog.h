/*
 * TuningDialog.h
 * --------------
 * Purpose: Alternative sample tuning configuration dialog.
 * Notes  : mfcless twin of upstream's mptrack/TuningDialog.h.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Window.h"
#include "mfcless/TreeCtrl.h"

#include "CDecimalSupport.h"
#include "ResizableDialog.h"
#include "tuningcollection.h"
#include "tuningRatioMapWnd.h"

class QAbstractButton;
class QComboBox;
class QLineEdit;
class QTreeWidgetItem;
class QWidget;

OPENMPT_NAMESPACE_BEGIN


// Tunings exist even outside of CSoundFile objects. We thus cannot use the
// GetCharsetInternal() encoding consistently. For now, just always treat
// tuning strings as Charset::Locale. As of OpenMPT 1.27, this distinction does
// not yet matter, because GetCharsetInteral() is always mpt::Charset::Locale if
// MODPLUG_TRACKER anyway.
extern const mpt::Charset TuningCharsetFallback;

template<class T1, class T2>
class CBijectiveMap
{
public:
	CBijectiveMap(const T1& a, const T2& b)
		:	m_NotFoundT1(a),
			m_NotFoundT2(b)
	{}

	void AddPair(const T1& a, const T2& b)
	{
		m_T1.push_back(a);
		m_T2.push_back(b);
	}

	void ClearMapping()
	{
		m_T1.clear();
		m_T2.clear();
	}

	size_t Size() const
	{
		MPT_ASSERT(m_T1.size() == m_T2.size());
		return m_T1.size();
	}

	void RemoveValue_1(const T1& a)
	{
		auto iter = find(m_T1.begin(), m_T1.end(), a);
		if(iter != m_T1.end())
		{
			m_T2.erase(m_T2.begin() + (iter-m_T1.begin()));
			m_T1.erase(iter);
		}
	}

	void RemoveValue_2(const T2& b)
	{
		auto iter = find(m_T2.begin(), m_T2.end(), b);
		if(iter != m_T2.end())
		{
			m_T1.erase(m_T1.begin() + (iter-m_T2.begin()));
			m_T2.erase(iter);
		}
	}

	T2 GetMapping_12(const T1& a) const
	{
		auto iter = find(m_T1.begin(), m_T1.end(), a);
		if(iter != m_T1.end())
		{
			return m_T2[iter-m_T1.begin()];
		}
		else
			return m_NotFoundT2;
	}

	T1 GetMapping_21(const T2& b) const
	{
		auto iter = find(m_T2.begin(), m_T2.end(), b);
		if(iter != m_T2.end())
		{
			return m_T1[iter-m_T2.begin()];
		}
		else
			return m_NotFoundT1;
	}

private:
	//Elements are collected to two arrays so that elements with the
	//same index are mapped to each other.
	std::vector<T1> m_T1;
	std::vector<T2> m_T2;

	T1 m_NotFoundT1;
	T2 m_NotFoundT2;
};

class CTuningDialog;

class CTuningTreeCtrl : public mfcless::TreeCtrl
{
	Q_OBJECT
private:
	CTuningDialog &m_rParentDialog;
	bool m_Dragging = false;

public:
	// mfcless: made by the dialog template, in the tuning dialog.
	explicit CTuningTreeCtrl(QWidget *parent);

	void SetDragging(bool state = true) {m_Dragging = state;}
	bool IsDragging() {return m_Dragging;}

	void OnMouseMove(uint32 nFlags, mfcless::Point point);
	void OnLButtonUp(uint32 nFlags, mfcless::Point point);
	void RegisterMessageMap() override;
};

class CTuningTreeItem
{
private:
	CTuning *m_pTuning = nullptr;
	CTuningCollection *m_pTuningCollection = nullptr;

public:
	CTuningTreeItem() = default;

	CTuningTreeItem(CTuning* pT) :
					m_pTuning(pT),
					m_pTuningCollection(nullptr)
	{}

	CTuningTreeItem(CTuningCollection* pTC) :
					m_pTuning(nullptr),
					m_pTuningCollection(pTC)
	{}

	bool operator==(const CTuningTreeItem& ti) const
	{
		if(m_pTuning == ti.m_pTuning &&
			m_pTuningCollection == ti.m_pTuningCollection)
			return true;
		else
			return false;
	}

	void Reset() {m_pTuning = nullptr; m_pTuningCollection = nullptr;}


	void Set(CTuning* pT)
	{
		m_pTuning = pT;
		m_pTuningCollection = nullptr;
	}

	void Set(CTuningCollection* pTC)
	{
		m_pTuning = nullptr;
		m_pTuningCollection = pTC;
	}

	operator bool () const
	{
		return m_pTuning || m_pTuningCollection;
	}
	bool operator ! () const
	{
		return !operator bool();
	}

	CTuningCollection* GetTC() {return m_pTuningCollection;}

	CTuning* GetT() {return m_pTuning;}
};

// CTuningDialog dialog

class CTuningDialog : public ResizableDialog
{
	Q_OBJECT
	friend class CTuningTreeCtrl;

	enum EnSclImport
	{
		enSclImportOk,
		enSclImportFailTooLargeNumDenomIntegers,
		enSclImportFailZeroDenominator,
		enSclImportFailNegativeRatio,
		enSclImportFailUnableToOpenFile,
		enSclImportLineCountMismatch,
		enSclImportTuningCreationFailure,
		enSclImportAddTuningFailure,
		enSclImportFailTooManyNotes
	};

public:
	using TUNINGVECTOR = std::vector<CTuningCollection*>;

public:
	CTuningDialog(QWidget *pParent, INSTRUMENTINDEX inst, CSoundFile &csf);
	~CTuningDialog() override;

	bool OnInitDialog() override;

	void UpdateRatioMapEdits(const Tuning::NOTEINDEXTYPE&);

	bool GetModifiedStatus(const CTuningCollection* const pTc) const;

protected:
	void DoDataExchange(DataExchange &dx) override;    // DDX/DDV support

private:

	bool CanEdit(CTuningCollection * pTC) const;
	bool CanEdit(CTuning * pT, CTuningCollection * pTC) const;

	void UpdateView(const int UpdateMask = 0);
	void UpdateTuningType();

	QTreeWidgetItem * AddTreeItem(CTuningCollection* pTC, QTreeWidgetItem * parent, QTreeWidgetItem * insertAfter);
	QTreeWidgetItem * AddTreeItem(CTuning* pT, QTreeWidgetItem * parent, QTreeWidgetItem * insertAfter);

	void DeleteTreeItem(CTuning* pT);
	void DeleteTreeItem(CTuningCollection* pTC);

	// Check if item can be dropped here. If yes, the target collection is returned, otherwise nullptr.
	CTuningCollection *CanDrop(QTreeWidgetItem * dragDestItem);
	void OnEndDrag(QTreeWidgetItem * dragDestItem);

	//Returns pointer to the tuning collection where tuning given as argument
	//belongs to.
	CTuningCollection* GetpTuningCollection(const CTuning* const) const;

	//Returns the address of corresponding tuningcollection; if it points
	//to tuning-entry, returning the owning tuningcollection
	CTuningCollection* GetpTuningCollection(QTreeWidgetItem * ti) const;

	//Checks whether tuning collection can be deleted.
	bool IsDeletable(const CTuningCollection* const pTC) const;

	// Scl-file import.
	EnSclImport ImportScl(const mpt::PathString &filename, const mpt::ustring &name, std::unique_ptr<CTuning> & result);
	EnSclImport ImportScl(std::istream& iStrm, const mpt::ustring &name, std::unique_ptr<CTuning> & result);


private:

	CSoundFile & m_sndFile;

	CTuningRatioMapWnd *m_RatioMapWnd = nullptr;
	TUNINGVECTOR m_TuningCollections;
	std::vector<CTuningCollection*> m_DeletableTuningCollections;

	std::map<const CTuningCollection*, mpt::ustring> m_TuningCollectionsNames;
	std::map<const CTuningCollection*, mpt::PathString> m_TuningCollectionsFilenames;

	CTuning *m_pActiveTuning = nullptr;
	CTuningCollection *m_pActiveTuningCollection = nullptr;

	QComboBox *m_CombobTuningType = nullptr;

	//Tuning Edits-->
	QLineEdit *m_EditSteps = nullptr;
	CNumberEdit *m_EditRatioPeriod = nullptr;
	CNumberEdit *m_EditRatio = nullptr;
	QLineEdit *m_EditNotename = nullptr;
	QLineEdit *m_EditMiscActions = nullptr;
	QLineEdit *m_EditFineTuneSteps = nullptr;
	QLineEdit *m_EditName = nullptr;
	//<--Tuning Edits

	QAbstractButton *m_ButtonSet = nullptr;
	QAbstractButton *m_ButtonNew = nullptr;
	QAbstractButton *m_ButtonExport = nullptr;
	QAbstractButton *m_ButtonImport = nullptr;
	QAbstractButton *m_ButtonRemove = nullptr;

	CTuningTreeCtrl *m_TreeCtrlTuning = nullptr;

private:
	using TUNINGTREEITEM = CTuningTreeItem;
	using TREETUNING_MAP = CBijectiveMap<QTreeWidgetItem *, TUNINGTREEITEM>;
	TREETUNING_MAP m_TreeItemTuningItemMap;

	TUNINGTREEITEM m_DragItem;
	TUNINGTREEITEM m_CommandItemSrc;
	TUNINGTREEITEM m_CommandItemDest;
	//Commanditem is used when receiving context menu-commands,
	//m_CommandItemDest is used when the command really need only
	//one argument.

	using MODIFIED_MAP = std::map<const CTuningCollection* const, bool>;
	MODIFIED_MAP m_ModifiedTCs;
	//If tuning collection seems to have been modified, its address
	//is added to this map.

	enum
	{
		TT_TUNINGCOLLECTION = 1,
		TT_TUNING
	};

	static mpt::ustring GetSclImportFailureMsg(EnSclImport);
	static constexpr size_t s_nSclImportMaxNoteCount = 256;

	//To indicate whether to apply changes made to
	//those edit boxes(they are modified by certain activities
	//in case which the modifications should not be applied to
	//tuning data.
	bool m_NoteEditApply = true;
	bool m_RatioEditApply = true;

	enum
	{
		UM_TUNINGDATA = 1, //UM <-> Update Mask
		UM_TUNINGCOLLECTION = 2,
	};

	static const TUNINGTREEITEM s_notFoundItemTuning;
	static QTreeWidgetItem *const s_notFoundItemTree;

	bool AddTuning(CTuningCollection*, CTuning* pT);
	bool AddTuning(CTuningCollection*, Tuning::Type type);

	//Flag to prevent multiple exit error-messages.
	bool m_DoErrorExit = false;

	void DoErrorExit();

	void OnOK() override;

//Treectrl context menu functions.
public:
	void OnRemoveTuning();
	void OnAddTuningGeneral();
	void OnAddTuningGroupGeometric();
	void OnAddTuningGeometric();
	void OnCopyTuning();
	void OnRemoveTuningCollection();

//Event-functions
public:
	void OnEnChangeEditSteps();
	void OnEnChangeEditRatioperiod();
	void OnEnChangeEditNotename();
	void OnBnClickedButtonSetvalues();
	void OnEnChangeEditRatiovalue();
	void OnBnClickedButtonNew();
	void OnBnClickedButtonExport();
	void OnBnClickedButtonImport();
	void OnBnClickedButtonRemove();
	void OnEnChangeEditFinetunesteps();
	void OnEnKillfocusEditFinetunesteps();
	void OnEnKillfocusEditName();
	void OnEnKillfocusEditSteps();
	void OnEnKillfocusEditRatioperiod();
	void OnEnKillfocusEditRatiovalue();
	void OnEnKillfocusEditNotename();

	//Treeview events
	void OnTvnSelchangedTreeTuning();
	void OnTvnDeleteitemTreeTuning();
	void OnNMRclickTreeTuning();
	void OnTvnBegindragTreeTuning();

	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
