// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "Controls/EditorDialog.h"

class QLineEdit;

//! Modal dialog with an edit field, used to prompt a string input from the user
//! Avoid using this if you can: prompting a string via modal dialog is a bad UX pattern we are trying to avoid
//! if you want to get a string, you should use some inline text edit instead of a dialog which is less disruptive
class EDITOR_COMMON_API QStringDialog : public CEditorDialog
{
	Q_OBJECT

public:
	QStringDialog(const QString& title, QWidget* pParent = NULL, bool bFileNameLimitation = false, bool bFileNameAsciiOnly = false);

	void SetString(const char* str);
	const string GetString();
	void SetCheckCallback(const std::function<bool(const string& str)>& check) { m_check = check; }

private:
	virtual void showEvent(QShowEvent* event) override;
	virtual void accept() override;

	QLineEdit* m_pEdit;
	std::function<bool(const string& str)> m_check;
	bool m_bFileNameLimitation;
	bool m_bFileNameAsciiOnly;
};


