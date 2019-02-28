// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// This is the source file for importing background objects into Mannequin.
// The recomended way to call this dialog is through DoModal() method.

#include "stdafx.h"
#include "MannImportBackgroundDialog.h"

//////////////////////////////////////////////////////////////////////////

CMannImportBackgroundDialog::CMannImportBackgroundDialog(std::vector<CString>& loadedObjects) :
	CDialog(IDD_MANN_IMPORT_BACKGROUND),
	m_loadedObjects(loadedObjects),
	m_selectedRoot(-1)
{
}

//////////////////////////////////////////////////////////////////////////

void CMannImportBackgroundDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ROOT_OBJECT, m_comboRoot);
}

//////////////////////////////////////////////////////////////////////////

BOOL CMannImportBackgroundDialog::OnInitDialog()
{
	if (__super::OnInitDialog() == FALSE)
		return FALSE;

	// Context combos
	const uint32 numObjects = m_loadedObjects.size();
	m_comboRoot.AddString("None");
	for (uint32 i = 0; i < numObjects; i++)
	{
		m_comboRoot.AddString(m_loadedObjects[i]);
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////

void CMannImportBackgroundDialog::OnOK()
{
	__super::OnOK();

	m_selectedRoot = m_comboRoot.GetCurSel() - 1;
}

//////////////////////////////////////////////////////////////////////////

