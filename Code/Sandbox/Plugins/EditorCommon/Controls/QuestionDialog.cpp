// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QuestionDialog.h"
#include "CryIcon.h"

#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

CQuestionDialog::CQuestionDialog()
	: CEditorDialog(QStringLiteral("QuestionDialog"), 0, false)
	, m_defaultButton(QDialogButtonBox::NoButton)
{
	setWindowTitle(tr(""));

	m_buttonPressed = QDialogButtonBox::StandardButton::NoButton;
	m_layout = new QGridLayout();

	m_infoLabel = new QLabel();
	m_infoLabel->setWordWrap(true);
	m_infoLabel->setMinimumWidth(400);

	m_iconLabel = new QLabel();
	m_iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	m_buttons = new QDialogButtonBox();
	m_buttons->setStandardButtons(QDialogButtonBox::Ok);

	connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_buttons, &QDialogButtonBox::clicked, this, &CQuestionDialog::ButtonClicked);

	setLayout(m_layout);
}

CQuestionDialog::~CQuestionDialog()
{
}

void CQuestionDialog::showEvent(QShowEvent* pEvent)
{
	CEditorDialog::showEvent(pEvent);

	// Set default button. This needs to be done after the dialog has been shown (see Qt docs).
	if (m_defaultButton != QDialogButtonBox::NoButton)
	{
		QPushButton* const pButton = m_buttons->button(m_defaultButton);
		if (pButton)
		{
			pButton->setDefault(true);
		}
	}
}

void CQuestionDialog::SetupUI(EInfoMessageType type, const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton)
{
	setWindowTitle(title);
	m_infoLabel->setText(text);
	m_buttons->setStandardButtons(buttons);

	m_defaultButton = defaultButton;

	if (!(buttons & QDialogButtonBox::StandardButton::Cancel))
	{
		SetDoNotClose();
	}

	int iconSize = 48;

	switch (type)
	{
	case EInfoMessageType::CriticalType:
		m_iconLabel->setPixmap(CryIcon("icons:Dialogs/dialog-error.ico").pixmap(iconSize, iconSize, QIcon::Normal, QIcon::On));
		break;
	case EInfoMessageType::QuestionType:
		m_iconLabel->setPixmap(CryIcon("icons:Dialogs/dialog-question.ico").pixmap(iconSize, iconSize, QIcon::Normal, QIcon::On));
		break;
	case EInfoMessageType::WarningType:
		m_iconLabel->setPixmap(CryIcon("icons:Dialogs/dialog-warning.ico").pixmap(iconSize, iconSize, QIcon::Normal, QIcon::On));
		break;
	}

	m_layout->addWidget(m_iconLabel, 0, 0, 2, 1, Qt::AlignRight | Qt::AlignVCenter);
	m_layout->addWidget(m_infoLabel, 0, 1, 2, 2, Qt::AlignLeft);

	for (auto& checkBox : m_checkBoxes)
	{
		auto row = m_layout->rowCount();
		m_layout->addWidget(checkBox.first, row, 0, 1, -1, Qt::AlignLeft);
	}

	auto row = m_layout->rowCount();
	m_layout->addWidget(m_buttons, row, 0, 1, -1, Qt::AlignRight);

	SetResizable(false);
}

void CQuestionDialog::SetupCritical(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton)
{
	SetupUI(EInfoMessageType::CriticalType, title, text, buttons, defaultButton);
}

void CQuestionDialog::SetupQuestion(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton)
{
	SetupUI(EInfoMessageType::QuestionType, title, text, buttons, defaultButton);
}

void CQuestionDialog::SetupWarning(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton)
{
	SetupUI(EInfoMessageType::WarningType, title, text, buttons, defaultButton);
}

void CQuestionDialog::AddCheckBox(const QString& text, bool* value)
{
	CRY_ASSERT(value);
	auto cb = new QCheckBox(text);
	cb->setChecked(*value);

	m_checkBoxes.append(QPair<QCheckBox*, bool*>(cb, value));
}

//This will also be called when pressing Enter for the default button
void CQuestionDialog::ButtonClicked(QAbstractButton* button)
{
	m_buttonPressed = m_buttons->standardButton(button);
	done(m_buttons->buttonRole(button));
}

QDialogButtonBox::StandardButton CQuestionDialog::Execute()
{
	auto result = exec();

	if (result == QDialog::Accepted)
	{
		for (auto& cb : m_checkBoxes)
		{
			*cb.second = cb.first->checkState() == Qt::Checked;
		}
	}

	if (m_buttonPressed == QDialogButtonBox::StandardButton::NoButton)
	{
		//Most likely cancelled by closing the window (or escape key)
		//Return "cancel" if we can, otherwise do not make assumptions
		if (m_buttons->standardButtons() & QDialogButtonBox::StandardButton::Cancel)
			return QDialogButtonBox::StandardButton::Cancel;
	}

	return m_buttonPressed;
}

// static calls
QDialogButtonBox::StandardButton CQuestionDialog::SCritical(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton)
{
	CQuestionDialog dialog;
	dialog.SetupUI(EInfoMessageType::CriticalType, title, text, buttons, defaultButton);
	return dialog.Execute();
}

QDialogButtonBox::StandardButton CQuestionDialog::SQuestion(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton)
{
	CQuestionDialog dialog;
	dialog.SetupUI(EInfoMessageType::QuestionType, title, text, buttons, defaultButton);
	return dialog.Execute();
}

QDialogButtonBox::StandardButton CQuestionDialog::SWarning(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton)
{
	CQuestionDialog dialog;
	dialog.SetupUI(EInfoMessageType::WarningType, title, text, buttons, defaultButton);
	return dialog.Execute();
}

QDialogButtonBox::StandardButton CQuestionDialog::SSave(const QString& title, const QString& text, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton)
{
	CQuestionDialog dialog;
	dialog.SetupUI(EInfoMessageType::QuestionType, title, text, buttons, defaultButton);
	return dialog.Execute();
}

