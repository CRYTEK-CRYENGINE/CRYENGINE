// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <CrySerialization/DynArray.h>

#include "Controls/EditorDialog.h"
class QListWidget;

//TODO : this is unused, use or remove
// Supposed to be used through
// ConfirmSaveDialog function.
class CUnsavedChangedDialog : public CEditorDialog
{
	Q_OBJECT
public:
	CUnsavedChangedDialog(QWidget* parent);

	bool Exec(DynArray<string>* selectedFiles, const DynArray<string>& files);
private:
	QListWidget* m_list;
	int          m_result;
};

// Returns true if window should be closed (Yes/No). False for Cancel.
// selectedFiles contains list of files that should be saved (empty in No case).
bool EDITOR_COMMON_API UnsavedChangesDialog(QWidget* parent, DynArray<string>* selectedFiles, const DynArray<string>& files);

