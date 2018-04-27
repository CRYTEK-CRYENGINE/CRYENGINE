// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/EditorDialog.h"

class QListView;
class QStandardItemModel;

class QFBXImporterDlg : public CEditorDialog
{
public:
	QFBXImporterDlg();

	void AddImportObject(string objectName);
	bool IsObjectSelected(string objectName);

protected:
	void OnConfirmed();

	QListView*          m_pObjectList;
	QStandardItemModel* m_pModel;

	bool                m_bConfirmed;
};

