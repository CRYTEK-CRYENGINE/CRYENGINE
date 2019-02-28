// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CreateFolderDialog.h"

#include "NameValidator.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QValidator>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CCreateFolderDialog::CCreateFolderDialog(QWidget* const pParent)
	: CEditorDialog("AudioCreateFolderDialog", pParent)
	, m_folderName("new_folder")
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle("Add Folder");

	auto const pLineEdit = new QLineEdit(m_folderName, this);
	auto const pValidator = new CNameValidator(s_regexInvalidFileName, pLineEdit);
	pLineEdit->setValidator(pValidator);
	pLineEdit->setMaxLength(64);
	pLineEdit->setMinimumWidth(150);
	pLineEdit->setToolTip(m_folderName);

	auto const pDialogButtons = new QDialogButtonBox(this);
	pDialogButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	auto const pLayout = new QVBoxLayout(this);
	setLayout(pLayout);
	pLayout->addWidget(pLineEdit);
	pLayout->addWidget(pDialogButtons);

	adjustSize();
	SetResizable(false);

	QObject::connect(pLineEdit, &QLineEdit::textChanged, [=](QString const& folderName)
	{
		QString fixedFolderName = folderName;
		pValidator->fixup(fixedFolderName);
		pDialogButtons->button(QDialogButtonBox::Ok)->setEnabled(!fixedFolderName.isEmpty());
		m_folderName = fixedFolderName;
		pLineEdit->setToolTip(fixedFolderName);
	});

	QObject::connect(pDialogButtons, &QDialogButtonBox::accepted, this, &CCreateFolderDialog::OnAccept);
	QObject::connect(pDialogButtons, &QDialogButtonBox::rejected, this, &CCreateFolderDialog::reject);
}

//////////////////////////////////////////////////////////////////////////
void CCreateFolderDialog::OnAccept()
{
	SignalSetFolderName(m_folderName);
	accept();
}
} // namespace ACE
