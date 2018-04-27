// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SaveChangesDialog.h"

#include <QLabel>
#include <QGridLayout>
#include <QAbstractButton>

#include "CryIcon.h"

CSaveChangesDialog::CSaveChangesDialog()
	: CEditorDialog(QStringLiteral("SaveChangesDialog"), nullptr, false)
{
	setWindowTitle(tr("Unsaved changes."));

	m_buttonPressed = QDialogButtonBox::StandardButton::Cancel;
	auto grid = new QGridLayout(this);

	const int iconSize = 48;
	m_iconLabel = new QLabel(this);
	m_iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_iconLabel->setPixmap(CryIcon("icons:Dialogs/dialog-warning.ico").pixmap(iconSize, iconSize, QIcon::Normal, QIcon::On));

	m_buttons = new QDialogButtonBox(this);
	m_buttons->setStandardButtons(QDialogButtonBox::Discard | QDialogButtonBox::Cancel);

	m_display = new QLabel();

	grid->addWidget(m_iconLabel, 0, 0, Qt::AlignRight | Qt::AlignVCenter);
	grid->addWidget(m_display, 0, 1, -1, -1);
	grid->addWidget(m_buttons, 1, 0, -1, -1, Qt::AlignRight);
	grid->setVerticalSpacing(20);

	connect(m_buttons, &QDialogButtonBox::clicked, this, &CSaveChangesDialog::ButtonClicked);

	SetResizable(true);
}

void CSaveChangesDialog::AddChangeList(const string& name, const std::vector<string>& changes)
{
	m_changeLists.emplace_back(name, changes);
}

void CSaveChangesDialog::FillText()
{
	QString text = "There are unsaved changes of the following files:\n\n";
	for (const auto& changeList : m_changeLists)
	{
		text += changeList.m_name + "\n";
		for (const auto& file : changeList.m_changes)
		{
			text.append(QString("    %1\n").arg(file.c_str()));
		}
		text += "\n";
	}
	m_display->setText(text);
}

void CSaveChangesDialog::ButtonClicked(QAbstractButton* button)
{
	m_buttonPressed = m_buttons->standardButton(button);
	accept();
}

QDialogButtonBox::StandardButton CSaveChangesDialog::GetButton() const
{
	return m_buttonPressed;
}

int CSaveChangesDialog::Execute()
{
	FillText();
	return exec();
}

