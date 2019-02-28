// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FRAGMENT_BROWSER__H__
#define __FRAGMENT_BROWSER__H__

#include <ICryMannequin.h>
#include "afxwin.h"
#include "Controls/ImageButton.h"

class CMannFragmentEditor;
struct SMannequinContexts;

class CFragmentTreeCtrl : public CXTTreeCtrl
{
protected:
	BOOL PreTranslateMessage(MSG* pMsg);
};

class CFragmentBrowser
	: public CXTResizeFormView
{
	typedef CXTResizeFormView PARENT;

	DECLARE_DYNAMIC(CFragmentBrowser)

	class CEditWithSelectAllOnLClick : public CEdit
	{
	protected:
		DECLARE_MESSAGE_MAP()
		afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	};

public:
	CFragmentBrowser(CMannFragmentEditor& fragEditor, CWnd* pParent, UINT nId);
	virtual ~CFragmentBrowser();

	enum { IDD = IDD_FRAGMENT_BROWSER };

	void SaveLayout(const XmlNodeRef& xmlLayout);
	void LoadLayout(const XmlNodeRef& xmlLayout);

	void Update(void);

	void SetContext(SMannequinContexts& context);

	void SetScopeContext(int scopeContextID);
	bool SelectFragments(const std::vector<FragmentID> fragmentIDs, const std::vector<SFragTagState>& tagStates, const std::vector<uint32>& options);
	bool SelectFragment(FragmentID fragmentID, const SFragTagState& tagState, uint32 option = 0);
	bool SetTagState(const std::vector<SFragTagState>& newTagsVec);

	typedef Functor0 OnScopeContextChangedCallback;
	void       SetOnScopeContextChangedCallback(OnScopeContextChangedCallback onScopeContextChangedCallback);

	FragmentID GetSelectedFragmentId() const;
	CString    GetSelectedNodeText() const;
	void       GetSelectedNodeTexts(std::vector<CString>& outTexts) const;
	void       GetSelectedNodeFullTexts(std::vector<CString>& outTexts) const;
	void       GetExpandedNodes(std::vector<CString>& outExpanedNodes) const;
	CString    GetNodePathName(const HTREEITEM item) const;

	void       SetADBFileNameTextToCurrent();

	int        GetScopeContextId() const
	{
		return m_cbContext.GetCurSel();
	}

	void                              RebuildAll();

	const std::vector<SFragTagState>& GetFragmentTagStates() const { return m_tagStates; }
	const std::vector<FragmentID>&    GetFragmentIDs() const       { return m_fragmentIDs; }

	void                              ClearSelectedItemData();

protected:
	virtual BOOL OnInitDialog();
	virtual void OnDestroy();
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual void OnOK();
	virtual void OnCancel() {}

	void         EditSelectedItem();
	void         SetEditItem(const std::vector<HTREEITEM>& boldItems, std::vector<FragmentID> fragmentIDs, const std::vector<SFragTagState>& tagStates, const std::vector<uint32>& options);
	void         SetDatabase(IAnimationDatabase* animDB);

	HTREEITEM    FindFragmentItem(FragmentID fragmentID, const SFragTagState& tagState, uint32 option) const;

	void         CleanFragmentID(FragmentID fragID);
	void         CleanFragment(FragmentID fragID);
	void         BuildFragment(FragmentID fragID);
	HTREEITEM    FindInChildren(const char* szTagStr, HTREEITEM parent);

	// Drag / drop helpers
	friend class CFragmentBrowserBaseDropTarget;
	FragmentID          GetValidFragIDForAnim(const CPoint& point, COleDataObject* pDataObject);
	bool                AddAnimToFragment(FragmentID fragID, COleDataObject* pDataObject);

	IAnimationDatabase* FindHostDatabase(uint32 contextID, const SFragTagState& tagState) const;

	CString             GetItemText(HTREEITEM item) const;

	struct STreeFragmentData
	{
		FragmentID    fragID;
		SFragTagState tagState;
		uint32        option;
		HTREEITEM     item;
		bool          tagSet;
	};

	COleDataSource* CreateFragmentDescriptionDataSource(STreeFragmentData* fragmentData) const;

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBeginDrag(NMHDR* pNMHdr, LRESULT* pResult);
	afx_msg void OnBeginRDrag(NMHDR* pNMHdr, LRESULT* pResult);

	afx_msg void OnTVClick(NMHDR*, LRESULT*);
	afx_msg void OnTVRightClick(NMHDR*, LRESULT*);
	afx_msg void OnTVEditSel(NMHDR*, LRESULT*);
	afx_msg void OnTreeKeyDown(NMHDR*, LRESULT*);
	afx_msg void OnTVSelChanged(NMHDR*, LRESULT*);
	afx_msg void OnChangeFilterTags();
	afx_msg void OnChangeFilterFragments();
	afx_msg void OnChangeFilterAnimClip();
	afx_msg void OnToggleSubfolders();
	afx_msg void OnToggleShowEmpty();
	afx_msg void OnNewBtn();
	afx_msg void OnEditBtn();
	afx_msg void OnDeleteBtn();
	afx_msg void OnTagDefEditorBtn();
	afx_msg void OnShowWindow(BOOL, UINT);
	afx_msg void OnChangeContext();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnNewDefinitionBtn();
	afx_msg void OnDeleteDefinitionBtn();
	afx_msg void OnRenameDefinitionBtn();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

	void         SelectFirstKey();

	uint32       AddNewFragment(FragmentID fragID, const SFragTagState& newTagState, const string& sAnimName);
private:

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	void         UpdateControlEnabledStatus();
	void         OnSelectionChanged();
	void         OnMenuCopy();
	void         OnMenuPaste(bool advancedPaste);
	bool         AddNewDefinition(bool bPasteAction = false);
	void         KeepCorrectData();

private:
	UINT                   m_nFragmentClipboardFormat;

	CFragmentTreeCtrl      m_TreeCtrl;
	CComboBox              m_cbContext;
	CEditWithSelectAllOnLClick m_editFilterTags;
	CEditWithSelectAllOnLClick m_editFilterFragmentIDs;
	CEditWithSelectAllOnLClick m_editAnimClipFilter;
	CButton                m_chkShowSubFolders;

	CButton                m_chkShowEmptyFolders;
	CImageButton           m_newEntry;
	CImageButton           m_deleteEntry;
	CImageButton           m_editEntry;
	CImageButton           m_newID;
	CImageButton           m_deleteID;
	CImageButton           m_renameID;
	CImageButton           m_tagDefEditor;
	CImageList             m_buttonImages;
	CToolTipCtrl           m_toolTip;

	bool                   m_showSubFolders;
	bool                   m_showEmptyFolders;

	CString                m_filterText;
	std::vector<CString>   m_filters;

	CString                m_filterFragmentIDText;

	CString                m_filterAnimClipText;

	IAnimationDatabase*    m_animDB;
	CMannFragmentEditor&   m_fragEditor;

	std::vector<HTREEITEM> m_boldItems;

	bool                   m_rightDrag;

	CImageList*            m_draggingImage;
	HTREEITEM              m_dragItem;

	struct SCopyItem
	{
		SCopyItem(const CFragment& _fragment, const SFragTagState _tagState) : fragment(_fragment), tagState(_tagState){}
		CFragment     fragment;
		SFragTagState tagState;
	};
	typedef std::vector<SCopyItem> TCopiedItems;
	TCopiedItems m_copiedItems;

	struct SCopyItemFragmentID
	{
		FragmentID   fragmentID; // this structure is only filled in when fragmentID != FRAGMENT_ID_INVALID

		SFragmentDef fragmentDef;
		string       subTagDefinitionFilename;

		SCopyItemFragmentID()
			: fragmentID(FRAGMENT_ID_INVALID)
		{
		}
	};
	SCopyItemFragmentID        m_copiedFragmentIDData;

	CImageList                 m_imageList;
	CEdit                      m_CurrFile;

	SMannequinContexts*        m_context;
	int                        m_scopeContext;

	FragmentID                 m_fragmentID;
	std::vector<FragmentID>    m_fragmentIDs;

	SFragTagState              m_tagState;
	std::vector<SFragTagState> m_tagStates;

	uint32                     m_option;
	std::vector<uint32>        m_options;

	typedef std::vector<STreeFragmentData*> TFragmentData;
	TFragmentData          m_fragmentData;

	std::vector<HTREEITEM> m_fragmentItems;

	float                  m_filterDelay;

	// Drag / Drop
	COleDropTarget*               m_pDropTarget;
	OnScopeContextChangedCallback m_onScopeContextChangedCallback;
};

#endif

