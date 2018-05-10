// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANN_TAG_TRANSITION_SETTINGS_H__
#define __MANN_TAG_TRANSITION_SETTINGS_H__
#pragma once

#include "MannequinBase.h"
#include "Controls/PropertiesPanel.h"

// fwd decl'
struct SScopeContextData;

class CMannTransitionSettingsDlg : public CXTResizeDialog
{
public:
	CMannTransitionSettingsDlg(const CString& windowCaption, FragmentID& fromFragID, FragmentID& toFragID, SFragTagState& fromFragTag, SFragTagState& toFragTag, CWnd* pParent = NULL);
	virtual ~CMannTransitionSettingsDlg();

	afx_msg void OnOk();
	afx_msg void OnComboChange();

protected:
	DECLARE_MESSAGE_MAP()

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

private:
	void UpdateFragmentIDs();
	void PopulateControls();
	void PopulateComboBoxes();
	void PopulateTagPanels();

	CString m_windowCaption;

	//
	FragmentID& m_IDFrom;
	FragmentID& m_IDTo;

	//
	SFragTagState& m_TagsFrom;
	SFragTagState& m_TagsTo;

	// "From" fragment ID selection
	CComboBox m_fromComboBox;

	// "To" fragment ID selection
	CComboBox m_toComboBox;

	// "From" tag editing
	CPropertiesPanel     m_tagsFromPanel;
	TSmartPtr<CVarBlock> m_tagFromVars;
	CTagControl          m_tagFromControls;
	CTagControl          m_tagFromFragControls;

	// "To" tag editing
	CPropertiesPanel     m_tagsToPanel;
	TSmartPtr<CVarBlock> m_tagToVars;
	CTagControl          m_tagToControls;
	CTagControl          m_tagToFragControls;
};

#endif

