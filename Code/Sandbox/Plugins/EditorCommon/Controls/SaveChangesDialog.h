// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorDialog.h"

#include <QDialogButtonBox>

class QLabel;

// Displays unsaved changes from multiple editors and only shows options "Discard" and "Cancel".
// A dialog more suitable for a single editor is in UnsavedChangesDialog.h.
class EDITOR_COMMON_API CSaveChangesDialog : public CEditorDialog
{
protected:
	struct SChangeList
	{
		string              m_name;
		std::vector<string> m_changes;

		SChangeList(const string& name, const std::vector<string>& changes)
			: m_name(name)
			, m_changes(changes)
		{}
	};
public:
	CSaveChangesDialog();

	void                             AddChangeList(const string& name, const std::vector<string>& changes);

	QDialogButtonBox::StandardButton GetButton() const;

	int                              Execute();
protected:
	void                             FillText();
	void                             ButtonClicked(QAbstractButton* button);

	std::vector<SChangeList>         m_changeLists;

	QLabel*                          m_display;
	QDialogButtonBox*                m_buttons;
	QLabel*                          m_iconLabel;
	QDialogButtonBox::StandardButton m_buttonPressed;
};

