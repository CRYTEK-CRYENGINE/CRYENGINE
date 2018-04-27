// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/EditorDialog.h"

#include <CryString/CryString.h>

#include <vector>

class CSingleSelectionDialog : public CEditorDialog
{
public:
	CSingleSelectionDialog(QWidget* pParent = nullptr);

	void SetOptions(const std::vector<string>& options);

	int GetSelectedIndex() const;

private:
	void Rebuild();

private:
	std::vector<string> m_options;
	int m_selectedOptionIndex;
};
