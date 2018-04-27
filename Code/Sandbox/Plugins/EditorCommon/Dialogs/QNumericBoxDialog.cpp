// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "QNumericBoxDialog.h"
#include <Controls/QNumericBox.h>
#include <QVBoxLayout>
#include <QDialogButtonBox>

QNumericBoxDialog::QNumericBoxDialog(const QString& title, QWidget* pParent /*= NULL*/)
	: CEditorDialog(QStringLiteral("QNumericBoxDialog"), pParent ? pParent : QApplication::widgetAt(QCursor::pos()), false)
{
	setWindowTitle(title.isEmpty() ? tr("Enter a number") : title);

	m_numBox = new QNumericBox(this);
	QDialogButtonBox* okCancelButton = new QDialogButtonBox(this);
	okCancelButton->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(okCancelButton, &QDialogButtonBox::accepted, this, &QNumericBoxDialog::accept);
	connect(okCancelButton, &QDialogButtonBox::rejected, this, &QNumericBoxDialog::reject);

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->addWidget(m_numBox);
	layout->addWidget(okCancelButton);

	setLayout(layout);
	SetResizable(false);
}

QNumericBoxDialog::QNumericBoxDialog(const QString& title, float defaultValue, QWidget* pParent /* nullptr */)
	: QNumericBoxDialog(title, pParent)
{
	SetValue(defaultValue);
}

void QNumericBoxDialog::SetValue(float num)
{
	m_numBox->setValue(num); 
}

float QNumericBoxDialog::GetValue() const
{
	return m_numBox->value(); 
}

void QNumericBoxDialog::SetStep(float step)
{
	m_numBox->setSingleStep(step);
}

float QNumericBoxDialog::GetStep() const
{
	return m_numBox->singleStep();
}

void QNumericBoxDialog::SetMin(float min)
{
	m_numBox->setMinimum(min);
}

void QNumericBoxDialog::SetMax(float max)
{
	m_numBox->setMaximum(max);
}

void QNumericBoxDialog::SetRange(float min, float max)
{
	m_numBox->setMinimum(min);
	m_numBox->setMaximum(max);
}

void QNumericBoxDialog::RestrictToInt()
{
	m_numBox->setRestrictToInt();
}

void QNumericBoxDialog::showEvent(QShowEvent* event)
{
	CEditorDialog::showEvent(event);
	m_numBox->grabFocus();
}

void QNumericBoxDialog::accept()
{
	if (m_check && !m_check(GetValue()))
		return;

	CEditorDialog::accept();
}


