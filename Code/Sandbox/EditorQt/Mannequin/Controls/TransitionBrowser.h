// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TRANSITION_BROWSER__H__
#define __TRANSITION_BROWSER__H__

#include <ICryMannequin.h>
#include "Controls/TreeCtrlReport.h"
#include "Controls/ImageButton.h"

struct SMannequinContexts;
class PreviewerPage;
class CTransitionEditorPage;
class CTransitionBrowser;

struct TTransitionID
{
	TTransitionID()
		: fromID(FRAGMENT_ID_INVALID)
		, toID(FRAGMENT_ID_INVALID)
	{
	}

	TTransitionID(const FragmentID fID, const FragmentID tID, const SFragTagState& fFragTag, const SFragTagState& tFragTag, const SFragmentBlendUid _uid)
		: fromID(fID)
		, toID(tID)
		, fromFTS(fFragTag)
		, toFTS(tFragTag)
		, uid(_uid)
	{
	}

	TTransitionID(const TTransitionID& transitionID)
		: fromID(transitionID.fromID)
		, toID(transitionID.toID)
		, fromFTS(transitionID.fromFTS)
		, toFTS(transitionID.toFTS)
		, uid(transitionID.uid)
	{
	}

	bool operator==(const TTransitionID& rhs) const
	{
		// If the uids match, the other fields should also match:
		CRY_ASSERT((uid != rhs.uid) || (fromID == rhs.fromID && toID == rhs.toID &&
		                                fromFTS == rhs.fromFTS && toFTS == rhs.toFTS));

		return uid == rhs.uid;
	}

	bool IsSameBlendVariant(const TTransitionID& rhs) const
	{
		return (fromID == rhs.fromID && toID == rhs.toID &&
		        fromFTS == rhs.fromFTS && toFTS == rhs.toFTS);
	}

	FragmentID        fromID, toID;
	SFragTagState     fromFTS, toFTS;
	SFragmentBlendUid uid;
};

//////////////////////////////////////////////////////////////////////////
// Item class for browser.
//////////////////////////////////////////////////////////////////////////
class CTransitionBrowserRecord : public CTreeItemRecord
{
	DECLARE_DYNAMIC(CTransitionBrowserRecord)

public:
	struct SRecordData
	{
		SRecordData()
		{
		}
		SRecordData(const CString& from, const CString& to);
		SRecordData(const TTransitionID& transitionID, float selectTime);

		bool operator==(const SRecordData& rhs) const
		{
			if (m_bFolder != rhs.m_bFolder)
				return false;

			if (m_bFolder)
				return (m_from == rhs.m_from) && (m_to == rhs.m_to);
			else
				return (m_transitionID == rhs.m_transitionID);
		}

		bool operator!=(const SRecordData& rhs) const
		{
			return !operator==(rhs);
		}

		CString       m_from;
		CString       m_to;
		bool          m_bFolder;
		TTransitionID m_transitionID; // only stored for the non-folders
	};

public:
	// create a folder
	CTransitionBrowserRecord(const CString& from, const CString& to);

	// create an entry that is not a folder
	CTransitionBrowserRecord(const TTransitionID& transitionID, float selectTime);

	inline CTransitionBrowserRecord* GetParent()
	{
		return (CTransitionBrowserRecord*)GetParentRecord();
	}

	const TTransitionID&      GetTransitionID() const;
	inline const SRecordData& GetRecordData() const
	{
		return m_data;
	}

	inline bool IsFolder() const
	{
		return m_data.m_bFolder;
	}

	bool IsEqualTo(const TTransitionID& transitionID) const;
	bool IsEqualTo(const SRecordData& data) const;

private:
	void CreateItems();

	SRecordData m_data;
};
typedef std::vector<CTransitionBrowserRecord::SRecordData> TTransitionBrowserRecordDataCollection;

//////////////////////////////////////////////////////////////////////////
// Tree class for browser.
//////////////////////////////////////////////////////////////////////////
class CTransitionBrowserTreeCtrl : public CTreeCtrlReport
{
public:
	typedef CTransitionBrowserRecord CRecord;

public:
	struct SPresentationState
	{
	private:
		TTransitionBrowserRecordDataCollection               expandedRecordDataCollection;
		TTransitionBrowserRecordDataCollection               selectedRecordDataCollection;
		std::auto_ptr<CTransitionBrowserRecord::SRecordData> pFocusedRecordData;

		friend class CTransitionBrowserTreeCtrl;
	};

public:
	static inline CRecord* RowToRecord(const CXTPReportRow* pRow)
	{
		if (!pRow)
			return 0;
		CXTPReportRecord* pRecord = pRow->GetRecord();
		if (!pRecord)
			return 0;
		if (!pRecord->IsKindOf(RUNTIME_CLASS(CRecord)))
			return 0;
		return (CRecord*)pRecord;
	}

public:
	CTransitionBrowserTreeCtrl(CTransitionBrowser* pTransitionBrowser);

	void     SelectItem(const TTransitionID& transitionID);

	CRecord* FindRecordForTransition(const TTransitionID& transitionID);

	void     SavePresentationState(OUT SPresentationState& outState) const;
	void     RestorePresentationState(const SPresentationState& state);

	void     SetBold(const TTransitionBrowserRecordDataCollection& recordDataCollection, const BOOL bBold = TRUE);

	//void PrintRows(const char* szWhen); // Was useful for debugging

protected:
	void                 SelectAndFocusRecordDataCollection(const CTransitionBrowserRecord::SRecordData* pFocusedRecordData, const TTransitionBrowserRecordDataCollection* pSelectedRecordDataCollection);
	void                 ExpandRowsForRecordDataCollection(const TTransitionBrowserRecordDataCollection& expandRecordDataCollection);
	CRecord*             SearchRecordForTransition(CRecord* pRecord, const TTransitionID& transitionID);
	void                 SelectItem(CTransitionBrowserRecord* pFocusedRecord);

	void                 SetBoldRecursive(CXTPReportRecords* pRecords, const TTransitionBrowserRecordDataCollection& recordDataCollection, const BOOL bBold);

	virtual afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) override;

protected:
	CTransitionBrowser* m_pTransitionBrowser;
};

//////////////////////////////////////////////////////////////////////////
// Transition Browser - displays a tree view of all available transitions.
//////////////////////////////////////////////////////////////////////////
class CTransitionBrowser : public CXTResizeFormView
{
	DECLARE_DYNAMIC(CTransitionBrowser)

public:
	CTransitionBrowser(CWnd* pParent, CTransitionEditorPage& transitionEditorPage);
	virtual ~CTransitionBrowser();

	enum
	{
		IDD = IDD_MANN_TRANSITION_BROWSER
	};

	void Refresh();
	void SetContext(SMannequinContexts& context);
	void SelectAndOpenRecord(const TTransitionID& transitionID);

	void FindFragmentReferences(CString fragSearchStr);
	void FindTagReferences(CString tagSearchStr);
	void FindClipReferences(CString icafSearchStr);

private:
	virtual BOOL              OnInitDialog() override;
	virtual void              DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL              PreTranslateMessage(MSG* pMsg) override;

	void                      OnSelectionChanged(CTreeItemRecord* pRec);
	void                      OnItemDoubleClicked(CTreeItemRecord* pRec);

	void                      UpdateButtonStatus();

	void                      SetDatabase(IAnimationDatabase* animDB);
	void                      SetScopeContextDataIdx(int scopeContextDataIdx);

	void                      SetOpenedRecord(CTransitionBrowserRecord* pRecordToOpen, const TTransitionID& pickedTransitionID);

	CTransitionBrowserRecord* GetSelectedRecord();
	bool                      GetSelectedTransition(TTransitionID& outTransitionID);

	void                      OpenSelected();
	void                      EditSelected();
	void                      CopySelected();
	void                      DeleteSelected();

	void                      OpenSelectedTransition(const TTransitionID& transitionID);
	bool                      ShowTransitionSettingsDialog(TTransitionID& inoutTransitionID, const CString& windowCaption);
	bool                      DeleteTransition(const TTransitionID& transitionToDelete); // returns true when the deleted transition was currently opened

	inline bool               HasOpenedRecord() const
	{
		return !m_openedRecordDataCollection.empty();
	}

	DECLARE_MESSAGE_MAP()

	afx_msg void OnNewBtn();
	afx_msg void OnEditBtn();
	afx_msg void OnCopyBtn();
	afx_msg void OnDeleteBtn();
	afx_msg void OnOpenBtn();
	afx_msg void OnChangeContext();
	afx_msg void OnSwapFragmentFilters();
	afx_msg void OnSwapTagFilters();
	afx_msg void OnSwapAnimClipFilters();

	afx_msg void OnChangeFilterTagsFrom();
	afx_msg void OnChangeFilterFragmentsFrom();
	afx_msg void OnChangeFilterAnimClipFrom();
	afx_msg void OnChangeFilterTagsTo();
	afx_msg void OnChangeFilterFragmentsTo();
	afx_msg void OnChangeFilterAnimClipTo();

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);

	void         UpdateTags(std::vector<CString>& tags, CString tagString);
	void         UpdateFilters();

	bool         FilterByAnimClip(const FragmentID fragId, const SFragTagState& tagState, const CString& filter);
private:
	CTransitionBrowserTreeCtrl             m_treeCtrl;
	CComboBox                              m_cbContext;

	IAnimationDatabase*                    m_animDB;
	CTransitionEditorPage&                 m_transitionEditorPage;

	CImageList                             m_imageList;

	CImageButton                           m_newButton;
	CImageButton                           m_editButton;
	CImageButton                           m_copyButton;
	CImageButton                           m_deleteButton;
	CImageButton                           m_openButton;
	CImageList                             m_buttonImages;
	CButton                                m_swapFragmentButton;
	CButton                                m_swapTagButton;
	CButton                                m_swapAnimClipButton;
	CToolTipCtrl                           m_toolTip;

	CEdit                                  m_editFilterTagsFrom;
	CEdit                                  m_editFilterFragmentIDsFrom;
	CEdit                                  m_editFilterAnimClipFrom;
	CEdit                                  m_editFilterTagsTo;
	CEdit                                  m_editFilterFragmentIDsTo;
	CEdit                                  m_editFilterAnimClipTo;

	CString                                m_filterTextFrom;
	std::vector<CString>                   m_filtersFrom;
	CString                                m_filterFragmentIDTextFrom;
	CString                                m_filterAnimClipFrom;

	CString                                m_filterTextTo;
	std::vector<CString>                   m_filtersTo;
	CString                                m_filterFragmentIDTextTo;
	CString                                m_filterAnimClipTo;

	SMannequinContexts*                    m_context;
	int                                    m_scopeContextDataIdx;

	TTransitionBrowserRecordDataCollection m_openedRecordDataCollection; // the record currently opened in the transition editor (followed by the parent records)
	TTransitionID                          m_openedTransitionID;         // This is NOT necessarily equal to the transitionID inside m_pOpenedRecord.
	// m_openedTransitionID is the blend actually opened, the blend the user
	// picked in the transition picker.
};

#endif

