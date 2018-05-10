// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANN_ADVANCED_PASTE_DIALOG__
#define __MANN_ADVANCED_PASTE_DIALOG__

#include <ICryMannequin.h>
#include "Controls/TagSelectionControl.h"

class CMannAdvancedPasteDialog : public CXTResizeDialog
{
public:
	CMannAdvancedPasteDialog(CWnd* pParent, const IAnimationDatabase& animDB, FragmentID fragmentID, const SFragTagState& tagState);

	enum { IDD = IDD_MANN_ADVANCED_PASTE };

	SFragTagState GetTagState() const { return m_tagState; }

protected:
	DECLARE_MESSAGE_MAP()

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();

private:
	const IAnimationDatabase& m_database;
	const FragmentID          m_fragmentID;
	SFragTagState             m_tagState;

	CStatic                   m_globalTagsGroupBox;
	CStatic                   m_fragTagsGroupBox;
	CTagSelectionControl      m_globalTagsSelectionControl;
	CTagSelectionControl      m_fragTagsSelectionControl;
};

#endif

