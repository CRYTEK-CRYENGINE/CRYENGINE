// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MannTagDefEditorDialog_h__
#define __MannTagDefEditorDialog_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <ICryMannequinEditor.h>
#include <ICryMannequinDefs.h>
#include "Controls/InPlaceButton.h"

class CTagDefinition;
class CTagDefTreeCtrl;

class CMannTagDefEditorDialog : public CXTResizeDialog
{
public:
	CMannTagDefEditorDialog(const CString& tagFilename = "", CWnd* pParent = NULL);
	virtual ~CMannTagDefEditorDialog();

	enum { IDD = IDD_MANN_TAG_DEFINITION_EDITOR };

protected:
	DECLARE_MESSAGE_MAP()

	typedef std::auto_ptr<CInPlaceButton> CInPlaceButtonPtr;
	friend class CTagDefTreeCtrl;

	struct STagDefPair
	{
		STagDefPair(const CTagDefinition* pOriginal, CTagDefinition* pCopy) : m_pOriginal(pOriginal), m_pCopy(pCopy), m_modified(false) {}

		const CTagDefinition* m_pOriginal;
		CTagDefinition*       m_pCopy;
		bool                  m_modified;

		struct SRenameInfo
		{
			CString m_newName;
			int32   m_originalCRC;
			bool    m_isGroup;
		};
		DynArray<SRenameInfo> m_renameInfo;
	};

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK() {};
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	void         CreateToolbarButton(CInPlaceButtonPtr& button, const char* text, CRect& rc, int index, int nID, CStatic* parent, CInPlaceButton::OnClick onClick);
	void         PopulateTagDefList();
	void         AddTagDefToList(CTagDefinition* pTagDef, bool select = false);
	HTREEITEM    AddGroupToTree(const char* szGroup, TagGroupID groupID, bool select = false);
	HTREEITEM    AddTagToTree(const char* szTag, TagID tagID, HTREEITEM parent, bool select = false);
	void         MoveTagInTree(HTREEITEM hItem, HTREEITEM hNewItem);
	void         EnableControls();
	void         PopulateTagDefTree();
	bool         ConfirmDelete(CString& text);
	void         AdjustTagsForDelete(TagID deletedTagID, HTREEITEM hItem = TVI_ROOT);
	void         AdjustGroupsForDelete(TagGroupID deletedGroupID);
	TagID        GetSelectedTagID();
	TagGroupID   GetSelectedTagGroupID();
	bool         IsTagTreeItem(HTREEITEM hItem, TagID& tagID);

	bool         ValidateTagName(const CString& name);
	bool         ValidateGroupName(const CString& name);
	bool         ValidateTagDefName(CString& name);

	afx_msg void OnTagDefListItemChanged(NMHDR*, LRESULT*);
	afx_msg void OnTagTreeSelChanged(NMHDR*, LRESULT*);
	afx_msg void OnTagTreeEndLabelEdit(NMHDR*, LRESULT*);
	afx_msg void OnPriorityChanged();
	afx_msg void OnPriorityEditChanged();
	afx_msg void OnOKButton();

private:
	void OnNewDefButton();
	void OnNewGroupButton();
	void OnEditGroupButton();
	void OnDeleteGroupButton();
	void OnNewTagButton();
	void OnEditTagButton();
	void OnDeleteTagButton();
	void OnFilterTagsButton();

	void PopulateTagDefListRec(const CString& baseDir);
	void RemoveUnmodifiedTagDefs(void);

	std::auto_ptr<CTagDefTreeCtrl> m_tagsTree;

	CStatic                        m_tagDefsGroupBox;
	CStatic                        m_tagsGroupBox;
	CListCtrl                      m_tagDefList;
	CImageList                     m_imageList;
	CImageList                     m_imageListToolbar;
	CStatic                        m_priorityLabel;
	CEdit                          m_priorityEdit;
	CEdit*                         m_pCurrentTagFile;

	CInPlaceButtonPtr              m_newDefButton;
	CInPlaceButtonPtr              m_newGroupButton;
	CInPlaceButtonPtr              m_editGroupButton;
	CInPlaceButtonPtr              m_deleteGroupButton;
	CInPlaceButtonPtr              m_newTagButton;
	CInPlaceButtonPtr              m_editTagButton;
	CInPlaceButtonPtr              m_deleteTagButton;
	CInPlaceButtonPtr              m_filterTagsButton;

	CToolTipCtrl*                  m_tooltip;
	CTagDefinition*                m_selectedTagDef;
	DynArray<STagDefPair>          m_tagDefLocalCopy;

	SSnapshotCollection            m_snapshotCollection;

	CString                        m_initialTagFilename;
};

#endif

