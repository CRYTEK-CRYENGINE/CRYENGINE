// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANN_ANIMDB_EDITOR_DIALOG_H__
#define __MANN_ANIMDB_EDITOR_DIALOG_H__
#pragma once

#include "MannequinBase.h"

class CTreeCtrlEx;

class CMannAnimDBEditorDialog : public CXTResizeDialog
{
public:
	CMannAnimDBEditorDialog(IAnimationDatabase* pAnimDB, CWnd* pParent = NULL);
	virtual ~CMannAnimDBEditorDialog();

	afx_msg void OnNewContext();
	afx_msg void OnEditContext();
	afx_msg void OnCloneAndEditContext();
	afx_msg void OnDeleteContext();

	afx_msg void OnMoveUp();
	afx_msg void OnMoveDown();

	afx_msg void OnSubADBTreeSelChanged(NMHDR*, LRESULT*);

protected:
	DECLARE_MESSAGE_MAP()

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

private:
	void         EnableControls();
	void         RetrieveTagNames(std::vector<string>& tagNames, const CTagDefinition& tagDef);
	void         PopulateSubADBTree();
	void         PopulateTreeSubADBFilters(const SMiniSubADB& miniSubADB, HTREEITEM parent);
	void         RefreshMiniSubADBs();
	SMiniSubADB* GetSelectedSubADBInternal(const string& text, SMiniSubADB* pSub);
	SMiniSubADB* GetSelectedSubADB();

	IAnimationDatabase*        m_animDB;

	SMiniSubADB::TSubADBArray  m_vSubADBs;

	CImageList                 m_imageList;
	std::auto_ptr<CTreeCtrlEx> m_adbTree;

	CButton                    m_newContext;
	CButton                    m_editContext;
	CButton                    m_cloneContext;
	CButton                    m_deleteContext;

	CButton                    m_moveUp;
	CButton                    m_moveDown;
};

#endif

