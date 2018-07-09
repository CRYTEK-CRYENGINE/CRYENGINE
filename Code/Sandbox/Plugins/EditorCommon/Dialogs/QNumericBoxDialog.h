// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "Controls/EditorDialog.h"

class QNumericBox;

//! Modal dialog with an edit field, used to prompt a numeric box for user to enter a number.
//! Avoid using this if you can: prompting a number via modal dialog is a bad UX pattern we are trying to avoid
//! if you want to get a number, you should use some inline text edit instead of a dialog which is less disruptive
class EDITOR_COMMON_API QNumericBoxDialog : public CEditorDialog
{
	Q_OBJECT

public:
	QNumericBoxDialog(const QString& title, QWidget* pParent = nullptr);
	QNumericBoxDialog(const QString& title, float defaultValue, QWidget* pParent = nullptr);

	void SetValue(float num);
	float GetValue() const;

	void SetStep(float step);
	float GetStep() const; 

	void SetMin(float min);
	void SetMax(float max);
	void SetRange(float min, float max);

	void RestrictToInt();

	void SetCheckCallback(const std::function<bool(float num)>& check) { m_check = check; }

private:
	virtual void showEvent(QShowEvent* event) override;
	virtual void accept() override;

	QNumericBox* m_numBox{ nullptr };
	std::function<bool(float num)> m_check;
};


