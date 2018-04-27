// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "QEditableComboBox.h"

#include "QMenuComboBox.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QEvent>
#include <QKeyEvent>

QEditableComboBox::QEditableComboBox(QWidget* pParent)
	: QWidget(pParent)
	, m_pLineEdit(new QLineEdit())
	, m_pComboBox(new QMenuComboBox())
{
	m_pComboBox->SetCanHaveEmptySelection(false);

	QHBoxLayout* pLayout = new QHBoxLayout();

	pLayout->addWidget(m_pLineEdit);
	pLayout->addWidget(m_pComboBox);
	m_pLineEdit->setVisible(false);
	m_pLineEdit->installEventFilter(this);

	connect(m_pLineEdit, &QLineEdit::editingFinished, this, &QEditableComboBox::OnEditingFinished);
	m_pComboBox->signalCurrentIndexChanged.Connect(this, &QEditableComboBox::OnCurrentIndexChanged);

	setLayout(pLayout);
}

bool QEditableComboBox::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pObject == m_pLineEdit && pEvent->type() == QEvent::KeyPress)
	{
		QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(pEvent);
		if (pKeyEvent->key() == Qt::Key_Escape)
		{
			OnEditingCancelled();
			return true;
		}
	}

	return false;
}

QString QEditableComboBox::GetCurrentText() const
{
	return m_pComboBox->GetText();
}

void QEditableComboBox::ClearItems()
{
	m_pComboBox->Clear();
}

void QEditableComboBox::AddItem(const QString& itemName)
{
	m_pComboBox->AddItem(itemName);
}

void QEditableComboBox::AddItems(const QStringList& itemNames)
{
	m_pComboBox->AddItems(itemNames);
}

void QEditableComboBox::RemoveCurrentItem()
{
	int idx = m_pComboBox->GetCheckedItem();

	if (idx > -1)
		m_pComboBox->RemoveItem(idx);
}

void QEditableComboBox::SetCurrentItem(const QString& itemName)
{
	m_pComboBox->SetChecked(itemName);
}

void QEditableComboBox::OnEditingCancelled()
{
	m_pComboBox->setVisible(true);
	m_pLineEdit->blockSignals(true);
	m_pLineEdit->setVisible(false);
	m_pLineEdit->blockSignals(false);
}

void QEditableComboBox::OnBeginEditing()
{
	m_pComboBox->setVisible(false);
	m_pLineEdit->setVisible(true);
	m_pLineEdit->setText(GetCurrentText());
	m_pLineEdit->selectAll();
	m_pLineEdit->setFocus(Qt::OtherFocusReason);
}

void QEditableComboBox::OnEditingFinished()
{
	m_pLineEdit->blockSignals(true);
	QString currentText = GetCurrentText();
	ItemRenamed(GetCurrentText(), m_pLineEdit->text());

	int currentIdx = m_pComboBox->GetCheckedItem();
	m_pComboBox->SetItemText(currentIdx, m_pLineEdit->text());

	m_pComboBox->setVisible(true);
	m_pLineEdit->setVisible(false);
	m_pLineEdit->blockSignals(false);
}

