// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __entityscriptdialog_h__
#define __entityscriptdialog_h__
#pragma once

// CEntityScriptDialog dialog

#include "Controls/ColorCheckBox.h"

class CEntityScript;

class CEntityScriptDialog : public CXTResizeDialog
{
	DECLARE_DYNAMIC(CEntityScriptDialog)
public:
	typedef Functor0 Callback;

	CEntityScriptDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CEntityScriptDialog();

	// Dialog Data
	enum { IDD = IDD_DB_ENTITY_METHODS };

	void SetScript(std::shared_ptr<CEntityScript> script, IEntity* m_entity);

	void SetOnReloadScript(Callback cb) { m_OnReloadScript = cb; };

protected:
	virtual void   DoDataExchange(CDataExchange* pDX);  // DDX/DDV support
	virtual BOOL   OnInitDialog();

	virtual void   OnOK()     {};
	virtual void   OnCancel() {};

	void           ReloadMethods();
	void           ReloadEvents();

	void           GotoMethod(const string& method);

	afx_msg void   OnEditScript();
	afx_msg void   OnReloadScript();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void   OnDblclkMethods();
	afx_msg void   OnGotoMethod();
	afx_msg void   OnAddMethod();
	afx_msg void   OnRunMethod();

	DECLARE_MESSAGE_MAP()

	CCustomButton m_addMethodBtn;
	CCustomButton                  m_runButton;
	CCustomButton                  m_gotoMethodBtn;
	CCustomButton                  m_editScriptBtn;
	CCustomButton                  m_reloadScriptBtn;
	CCustomButton                  m_removeButton;
	CStatic                        m_scriptName;
	CListBox                       m_methods;
	CString                        m_selectedMethod;

	std::shared_ptr<CEntityScript> m_script;
	IEntity*                       m_entity;
	CBrush                         m_grayBrush;

	Callback                       m_OnReloadScript;
};

#endif // __entityscriptdialog_h__

