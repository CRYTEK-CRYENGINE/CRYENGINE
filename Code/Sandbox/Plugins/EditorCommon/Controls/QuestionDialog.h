// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "EditorCommonAPI.h"
#include "Controls/EditorDialog.h"
#include <QDialogButtonbox>

class QLabel;
class QCheckBox;
class QGridLayout;

enum EInfoMessageType
{
	CriticalType,
	QuestionType,
	WarningType
};

class EDITOR_COMMON_API CQuestionDialog : public CEditorDialog
{
	Q_OBJECT
public:
	CQuestionDialog();
	~CQuestionDialog();

	//will set the boolean value only if the dialog is accepted
	//must be called before "setup"
	void AddCheckBox(const QString& text, bool* value);

	void SetupCritical(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::No);
	void SetupQuestion(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::Yes);
	void SetupWarning(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::No);

	//! Use this method to display the dialog and wait for answer, never use any other method to display the dialog from QDialog otherwise the behavior is not guaranteed
	QDialogButtonBox::StandardButton Execute();

public:

	// static calls. Prefer using this for trivial cases
	static QDialogButtonBox::StandardButton SCritical(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::No);
	static QDialogButtonBox::StandardButton SQuestion(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::No);
	static QDialogButtonBox::StandardButton SWarning(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::No);
	//! Shows a standard save dialog with the options to save, cancel or discard
	static QDialogButtonBox::StandardButton SSave(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Save | QDialogButtonBox::Discard | QDialogButtonBox::Cancel, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::Cancel);

protected:
	virtual void showEvent(QShowEvent* pEvent) override;

	void SetupUI(EInfoMessageType type, const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::No);
	void ButtonClicked(QAbstractButton* button);

	QLabel*                           m_infoLabel;
	QDialogButtonBox*                 m_buttons;
	QLabel*                           m_iconLabel;
	QDialogButtonBox::StandardButton  m_buttonPressed;
	QDialogButtonBox::StandardButton  m_defaultButton;
	QGridLayout*                      m_layout;
	QVector<QPair<QCheckBox*, bool*>> m_checkBoxes;
};

