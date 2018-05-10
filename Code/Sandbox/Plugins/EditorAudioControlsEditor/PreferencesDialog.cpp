// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PreferencesDialog.h"

#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"

#include <ISettings.h>
#include <QtUtil.h>
#include <FileDialogs/SystemFileDialog.h>

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
{
	Impl::ISettings const* const pISettings = g_pIImpl->GetSettings();

	if (pISettings != nullptr)
	{
		setWindowTitle(tr("Audio System Preferences"));

		auto const pMainLayout = new QVBoxLayout(this);
		setLayout(pMainLayout);

		auto const pLayout = new QGridLayout(this);

		Qt::Alignment const labelAlignment = static_cast<Qt::Alignment>(Qt::AlignLeft | Qt::AlignVCenter);

		pLayout->addWidget(new QLabel(tr("Audio Middleware") + ":"), 0, 0, labelAlignment);
		pLayout->addWidget(new QLabel(QtUtil::ToQString(g_pIImpl->GetName())), 0, 1, labelAlignment);

		pLayout->addWidget(new QLabel(tr("Assets Path") + ":"), 1, 0, labelAlignment);
		pLayout->addWidget(new QLabel(pISettings->GetAssetsPath()), 1, 1);

		pLayout->addWidget(new QLabel(tr("Project Path") + ":"), 2, 0, labelAlignment);

		auto const pProjectPathLayout = new QHBoxLayout(this);
		m_projectPath = pISettings->GetProjectPath();
		auto const pLineEdit = new QLineEdit(m_projectPath, this);
		pLineEdit->setMinimumWidth(250);

		auto const pBrowseButton = new QToolButton(this);
		pBrowseButton->setText("...");

		if (pISettings->SupportsProjects())
		{
			QObject::connect(pLineEdit, &QLineEdit::textChanged, [=](QString const& projectPath)
				{
					SignalEnableSaveButton(m_projectPath.toLower().replace("/", R"(\)") != projectPath.toLower().replace("/", R"(\)"));
			  });

			QObject::connect(pBrowseButton, &QToolButton::clicked, [=]()
				{
					CSystemFileDialog::RunParams runParams;
					runParams.initialDir = pLineEdit->text();
					QString const& path = CSystemFileDialog::RunSelectDirectory(runParams, this);

					if (!path.isEmpty())
					{
					  pLineEdit->setText(path);
					}
			  });
		}
		else
		{
			pLineEdit->setText(tr("The selected middleware doesn't support projects."));
			pLineEdit->setEnabled(false);
			pBrowseButton->setEnabled(false);
		}

		pProjectPathLayout->addWidget(pLineEdit);
		pProjectPathLayout->addWidget(pBrowseButton);

		pLayout->addLayout(pProjectPathLayout, 2, 1);

		pMainLayout->addLayout(pLayout);

		auto const pButtons = new QDialogButtonBox(this);
		pButtons->setStandardButtons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
		pButtons->button(QDialogButtonBox::Save)->setEnabled(false);
		QObject::connect(this, &CPreferencesDialog::SignalEnableSaveButton, pButtons->button(QDialogButtonBox::Save), &QPushButton::setEnabled);
		QObject::connect(pButtons, &QDialogButtonBox::accepted, [=]()
			{
				Impl::ISettings* const pISettings = g_pIImpl->GetSettings();

				if (pISettings != nullptr)
				{
				  pISettings->SetProjectPath(QtUtil::ToString(pLineEdit->text()));
				  SignalImplementationSettingsAboutToChange();
				  CAudioControlsEditorPlugin::ReloadData(EReloadFlags::ReloadImplData | EReloadFlags::BackupConnections);
				  SignalImplementationSettingsChanged();
				}

				accept();
		  });

		QObject::connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		pMainLayout->addWidget(pButtons, 0);

		adjustSize();
		SetResizable(false);
	}
}
} // namespace ACE

