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
			setWindowTitle(QtUtil::ToQString(pAudioSystem->GetName()) + " Settings");

			QVBoxLayout* pMainLayout = new QVBoxLayout(this);
			setLayout(pMainLayout);

			QGridLayout* pLayout = new QGridLayout(this);

			pLayout->addWidget(new QLabel(tr("Sound Banks Path") + ":"), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
			pLayout->addWidget(new QLabel(pSettings->GetSoundBanksPath()), 0, 1);

			pLayout->addWidget(new QLabel(tr("Project Path") + ":"), 1, 0, Qt::AlignRight | Qt::AlignVCenter);

			QHBoxLayout* pProjectPathLayout = new QHBoxLayout(this);
			QLineEdit* pLineEdit = new QLineEdit(pSettings->GetProjectPath());
			pLineEdit->setMinimumWidth(200);
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

			pLayout->addLayout(pProjectPathLayout, 1, 1);

			pMainLayout->addLayout(pLayout);

			QWidget* pHorizontalLine = new QWidget;
			pHorizontalLine->setFixedHeight(1);
			pHorizontalLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			pHorizontalLine->setStyleSheet(QString("background-color: #505050;"));
			pMainLayout->addWidget(pHorizontalLine);

			QDialogButtonBox* pButtons = new QDialogButtonBox(this);
			pButtons->setStandardButtons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
			pButtons->adjustSize();
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

			pMainLayout->setSizeConstraint(QLayout::SetFixedSize);

		}
	}
}
}
