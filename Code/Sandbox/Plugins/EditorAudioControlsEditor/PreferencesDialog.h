// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/EditorDialog.h>

namespace ACE
{
class CPreferencesDialog final : public CEditorDialog
{
	Q_OBJECT

public:

	explicit CPreferencesDialog(QWidget* pParent);

	CPreferencesDialog() = delete;

signals:

	void SignalOnBeforeImplSettingsChange();
	void SignalOnAfterImplSettingsChanged();
	void SignalEnableSaveButton(bool);

private:

	QString const m_projectPath;
};
} // namespace ACE
