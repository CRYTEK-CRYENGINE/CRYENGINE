// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANN_NEW_SUBADB_FILTER_DIALOG_H__
#define __MANN_NEW_SUBADB_FILTER_DIALOG_H__
#pragma once

#include "MannequinBase.h"
#include "Controls/PropertiesPanel.h"

class CMannNewSubADBFilterDialog : public CXTResizeDialog
{
public:
	enum EContextDialogModes
	{
		eContextDialog_New = 0,
		eContextDialog_Edit,
		eContextDialog_CloneAndEdit
	};
	CMannNewSubADBFilterDialog(SMiniSubADB* context, IAnimationDatabase* animDB, const string& parentName, const EContextDialogModes mode, CWnd* pParent = NULL);
	virtual ~CMannNewSubADBFilterDialog();

	afx_msg void OnOk();
	afx_msg void OnFrag2Used();
	afx_msg void OnUsed2Frag();
	afx_msg void OnFilenameChanged();

	afx_msg void OnFragListItemChanged(NMHDR*, LRESULT*);
	afx_msg void OnUsedListItemChanged(NMHDR*, LRESULT*);

protected:
	DECLARE_MESSAGE_MAP()

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

private:
	void EnableControls();
	bool VerifyFilename();
	void PopulateControls();
	void PopulateFragIDList();
	void CloneSubADBData(const SMiniSubADB* context);
	void CopySubADBData(const SMiniSubADB* pData);
	void SaveNewSubADBFile(const string& filename);
	void FindFragmentIDsMissing(SMiniSubADB::TFragIDArray& outFrags, const SMiniSubADB::TFragIDArray& AFrags, const SMiniSubADB::TFragIDArray& BFrags);

	//bool m_editing;
	const EContextDialogModes m_mode;
	SMiniSubADB*              m_pData;
	SMiniSubADB*              m_pDataCopy;
	IAnimationDatabase*       m_animDB;
	const string&             m_parentName;
	int                       m_selectFraglistIndex;
	int                       m_selectUsedlistIndex;

	CButton                   m_okButton;
	CEdit                     m_nameEdit;
	CListCtrl                 m_fragIDList;
	CListCtrl                 m_fragUsedIDList;
	CPropertiesPanel          m_tagsPanel;

	TSmartPtr<CVarBlock>      m_tagVars;
	CTagControl               m_tagControls;
};

#endif

