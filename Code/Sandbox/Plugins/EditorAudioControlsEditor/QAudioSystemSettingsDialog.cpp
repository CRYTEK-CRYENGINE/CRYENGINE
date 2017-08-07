// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QAudioSystemSettingsDialog.h"
#include "AudioControlsEditorPlugin.h"
#include "IAudioSystemEditor.h"
#include <QtUtil.h>

// Qt
#include <QGridLayout>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include "FileDialogs/SystemFileDialog.h"

namespace ACE
{
QAudioSystemSettingsDialog::QAudioSystemSettingsDialog(QWidget* pParent)
	: CEditorDialog("AudioSystemSettingsDialog")
{
	IAudioSystemEditor* pAudioSystem = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
	if (pAudioSystem)
	{
		IImplementationSettings* pSettings = pAudioSystem->GetSettings();
		if (pSettings)
		{
			setWindowTitle(tr("Audio System Settings"));

			QVBoxLayout* pMainLayout = new QVBoxLayout(this);
			setLayout(pMainLayout);

			QGridLayout* pLayout = new QGridLayout(this);

			pLayout->addWidget(new QLabel(tr("Audio Middleware") + ":"), 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
			pLayout->addWidget(new QLabel(QtUtil::ToQString(pAudioSystem->GetName())), 0, 1, Qt::AlignLeft | Qt::AlignVCenter);

			pLayout->addWidget(new QLabel(tr("Sound Banks Path") + ":"), 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
			pLayout->addWidget(new QLabel(pSettings->GetSoundBanksPath()), 1, 1);

			pLayout->addWidget(new QLabel(tr("Project Path") + ":"), 2, 0, Qt::AlignLeft | Qt::AlignVCenter);

			QHBoxLayout* pProjectPathLayout = new QHBoxLayout(this);
			QLineEdit* pLineEdit = new QLineEdit(pSettings->GetProjectPath());
			pLineEdit->setMinimumWidth(250);
			pProjectPathLayout->addWidget(pLineEdit);

			QToolButton* pBrowseButton = new QToolButton();
			pBrowseButton->setText("...");
			connect(pBrowseButton, &QToolButton::clicked, [=]()
				{
					IAudioSystemEditor* pAudioSystem = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
					if (pAudioSystem)
					{
					  CSystemFileDialog::RunParams runParams;
					  runParams.initialDir = pLineEdit->text();
					  QString path = CSystemFileDialog::RunSelectDirectory(runParams, this);
					  if (!path.isEmpty())
					  {
					    pLineEdit->setText(path);
					  }
					}
			  });
			pProjectPathLayout->addWidget(pBrowseButton);

			pLayout->addLayout(pProjectPathLayout, 2, 1);

			pMainLayout->addLayout(pLayout);

			QDialogButtonBox* pButtons = new QDialogButtonBox(this);
			pButtons->setStandardButtons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
			connect(pButtons, &QDialogButtonBox::accepted, [=]()
				{
					IAudioSystemEditor* pAudioSystem = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
					if (pAudioSystem)
					{
					  IImplementationSettings* pSettings = pAudioSystem->GetSettings();
					  if (pSettings)
					  {

					    pSettings->SetProjectPath(QtUtil::ToString(pLineEdit->text()));

					    // clear all connections to the middleware since we are reloading everything
					    CAudioControlsEditorPlugin::GetAssetsManager()->ClearAllConnections();
					    pAudioSystem->Reload();
					    CAudioControlsEditorPlugin::GetAssetsManager()->ReloadAllConnections();

					  }
					}
					accept();
			  });
			connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
			pMainLayout->addWidget(pButtons, 0);

			adjustSize();
			SetResizable(false);
		}
	}
}
} // namespace ACE
