// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANN_NEW_CONTEXT_DIALOG_H__
#define __MANN_NEW_CONTEXT_DIALOG_H__
#pragma once

#include "MannequinBase.h"
#include "Controls/PropertiesPanel.h"

// fwd decl'
struct SScopeContextData;

class CMannNewContextDialog : public CXTResizeDialog
{
public:
	enum EContextDialogModes
	{
		eContextDialog_New = 0,
		eContextDialog_Edit,
		eContextDialog_CloneAndEdit
	};
	CMannNewContextDialog(SScopeContextData* context, const EContextDialogModes mode, CWnd* pParent = NULL);
	virtual ~CMannNewContextDialog();

	afx_msg void OnOk();
	afx_msg void OnBrowsemodelButton();
	afx_msg void OnNewAdbButton();

protected:
	DECLARE_MESSAGE_MAP()

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

private:
	void PopulateControls();
	void PopulateDatabaseCombo();
	void PopulateControllerDefCombo();

	void CloneScopeContextData(const SScopeContextData* context);

	//bool m_editing;
	const EContextDialogModes m_mode;
	SScopeContextData*        m_pData;

	CButton                   m_startActiveCheckBox;
	CEdit                     m_nameEdit;
	CComboBox                 m_contextCombo;
	CPropertiesPanel          m_tagsPanel;
	CEdit                     m_fragTagsEdit;
	CEdit                     m_modelEdit;
	CButton                   m_browseButton;
	CComboBox                 m_databaseCombo;
	CComboBox                 m_attachmentContextCombo;
	CEdit                     m_attachmentEdit;
	CEdit                     m_attachmentHelperEdit;
	CEdit                     m_startPosXEdit;
	CEdit                     m_startPosYEdit;
	CEdit                     m_startPosZEdit;
	CEdit                     m_startRotXEdit;
	CEdit                     m_startRotYEdit;
	CEdit                     m_startRotZEdit;
	CComboBox                 m_slaveControllerDefCombo;
	CComboBox                 m_slaveDatabaseCombo;

	TSmartPtr<CVarBlock>      m_tagVars;
	CTagControl               m_tagControls;
};

#endif

