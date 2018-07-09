// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AboutDialog.h"

#include <QLayout>
#include <QLabel>
#include <QPushButton>

CAboutDialog::CAboutDialog()
	: CEditorDialog("About", nullptr, false)
{
	setModal(true);
	QVBoxLayout* layout = new QVBoxLayout();
	setLayout(layout);

	m_version = GetIEditorImpl()->GetFileVersion();
	m_versionText = new QLabel();
	SetVersion(m_version);

	m_miscInfoLabel = new QLabel();
	m_miscInfoLabel->setText(tr("CRYENGINE(R) Sandbox(TM) Editor\nCopyright(c) 2018, Crytek GmbH\n\nCrytek, Crytek logo, CRYENGINE, Sandbox are trademarks of Crytek.\n"));

	QPixmap* logo = new QPixmap(":/about.png");
	QLabel* logoImg = new QLabel();
	logoImg->setPixmap(*logo);

	QPushButton* closeBtn = new QPushButton(tr("&Close"));

	connect(closeBtn, &QPushButton::clicked, [this]()
	{
		close();
	});

	QHBoxLayout* bottomLayout = new QHBoxLayout();
	bottomLayout->addStretch(1);
	bottomLayout->addWidget(closeBtn);

	layout->addWidget(logoImg, 0, Qt::AlignCenter);
	layout->addWidget(m_miscInfoLabel);
	layout->addWidget(m_versionText);
	layout->addLayout(bottomLayout);

	SetResizable(false);
}

CAboutDialog::~CAboutDialog()
{
	close();
}

void CAboutDialog::SetVersion(const Version& version)
{
	char versionText[256];
	cry_sprintf(versionText, "Version %d.%d.%d - Build %d", version[3], version[2], version[1], version[0]);
	m_versionText->setText(versionText);
}

