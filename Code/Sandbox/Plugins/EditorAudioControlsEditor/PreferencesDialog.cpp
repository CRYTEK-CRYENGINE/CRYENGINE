// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PreferencesDialog.h"

#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"
#include "NameValidator.h"

#include <QtUtil.h>
#include <FileDialogs/SystemFileDialog.h>
#include <CryIcon.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CPreferencesDialog::CPreferencesDialog(QWidget* const pParent)
	: CEditorDialog("AudioSystemPreferencesDialog", pParent)
	, m_projectPath(g_pIImpl->GetProjectPath())
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(tr("Audio System Preferences"));

	auto const pMainLayout = new QVBoxLayout(this);
	setLayout(pMainLayout);

	auto const pLabelLayout = new QGridLayout(this);
	auto const labelAlignment = static_cast<Qt::Alignment>(Qt::AlignLeft | Qt::AlignVCenter);

	pLabelLayout->addWidget(new QLabel(tr("Audio Middleware") + ":"), 0, 0, labelAlignment);
	pLabelLayout->addWidget(new QLabel(QtUtil::ToQString(g_pIImpl->GetName())), 0, 1, labelAlignment);

	pLabelLayout->addWidget(new QLabel(tr("Assets Path") + ":"), 1, 0, labelAlignment);
	pLabelLayout->addWidget(new QLabel(g_pIImpl->GetAssetsPath()), 1, 1);

	pLabelLayout->addWidget(new QLabel(tr("Project Path") + ":"), 2, 0, labelAlignment);

	auto const pLineEdit = new QLineEdit(m_projectPath, this);
	auto const pValidator = new CNameValidator(s_regexInvalidFilePath, pLineEdit);
	pLineEdit->setValidator(pValidator);
	pLineEdit->setMinimumWidth(300);
	pLineEdit->setToolTip(m_projectPath);

	auto const pBrowseButton = new QToolButton(this);
	pBrowseButton->setIcon(CryIcon("icons:General/Folder.ico"));
	pBrowseButton->setToolTip(tr("Select project path"));
	pBrowseButton->setIconSize(QSize(16, 16));

	if (g_pIImpl->SupportsProjects())
	{
		QObject::connect(pLineEdit, &QLineEdit::textChanged, [=](QString const& projectPath)
			{
				QString fixedProjectPath = projectPath;
				pValidator->fixup(fixedProjectPath);
				SignalEnableSaveButton(!fixedProjectPath.isEmpty() && (m_projectPath.toLower().replace("/", R"(\)") != fixedProjectPath.toLower().replace("/", R"(\)")));
				pLineEdit->setToolTip(fixedProjectPath);
		  });

		QObject::connect(pBrowseButton, &QToolButton::clicked, [=]()
			{
				CSystemFileDialog::RunParams runParams;
				runParams.initialDir = pLineEdit->text();
				QString const& path = CSystemFileDialog::RunSelectDirectory(runParams, this);

				if (!path.isEmpty())
				{
				  pLineEdit->setText(path);
				  pLineEdit->setToolTip(path);
				}
		  });
	}
	else
	{
		QString const noProjectSupportedText = tr("The selected middleware doesn't support projects.");
		pLineEdit->setText(noProjectSupportedText);
		pLineEdit->setToolTip(noProjectSupportedText);
		pLineEdit->setEnabled(false);
		pBrowseButton->setEnabled(false);
	}

	auto const pProjectPathLayout = new QHBoxLayout(this);
	pProjectPathLayout->addWidget(pLineEdit);
	pProjectPathLayout->addWidget(pBrowseButton);

	pLabelLayout->addLayout(pProjectPathLayout, 2, 1);

	pMainLayout->addLayout(pLabelLayout);

	auto const pButtons = new QDialogButtonBox(this);
	pButtons->setStandardButtons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
	pButtons->button(QDialogButtonBox::Save)->setEnabled(false);
	QObject::connect(this, &CPreferencesDialog::SignalEnableSaveButton, pButtons->button(QDialogButtonBox::Save), &QPushButton::setEnabled);
	QObject::connect(pButtons, &QDialogButtonBox::accepted, [=]()
		{
			QString fixedProjectPath = pLineEdit->text();
			pValidator->fixup(fixedProjectPath);
			g_pIImpl->SetProjectPath(QtUtil::ToString(fixedProjectPath));
			SignalImplementationSettingsAboutToChange();
			CAudioControlsEditorPlugin::ReloadData(EReloadFlags::ReloadImplData | EReloadFlags::BackupConnections);
			SignalImplementationSettingsChanged();

			accept();
	  });

	QObject::connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	pMainLayout->addWidget(pButtons, 0);

	adjustSize();
	SetResizable(false);
}
} // namespace ACE
