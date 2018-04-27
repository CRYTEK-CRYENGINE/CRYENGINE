// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QResourceBrowserDialog.h"
#include "QObjectTreeWidget.h"

#include <QDirIterator>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QFileInfo>

QResourceBrowserDialog::QResourceBrowserDialog()
	: CEditorDialog("Editor Resources Browser")
{
	QVBoxLayout* pMainLayout = new QVBoxLayout();
	QObjectTreeWidget* pTreeWidget = new QObjectTreeWidget(this, "[/\\\\]");
	QString alias("icons:");
	QDirIterator it(alias, QDirIterator::Subdirectories);
	QString basePath(":/icons/");
	while (it.hasNext())
	{
		QString path = it.next();
		if (QFileInfo(path).isDir())
			continue;

		QString subPath = path.mid(path.indexOf(basePath) + basePath.length(), path.length());
		QString id = alias + subPath;
		pTreeWidget->AddEntry(subPath.toStdString().c_str(), id.toStdString().c_str());
	}
	pTreeWidget->setWindowFlags(Qt::Window);
	pTreeWidget->show();

	pTreeWidget->signalOnDoubleClickFile.Connect(this, &QResourceBrowserDialog::ResourceSelected);
	pTreeWidget->signalOnClickFile.Connect(this, &QResourceBrowserDialog::PreviewResource);

	QPushButton* pOkButton = new QPushButton(this);
	pOkButton->setText(tr("Ok"));
	pOkButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
	pOkButton->setDefault(true);
	connect(pOkButton, &QPushButton::clicked, [this, pTreeWidget]()
	{
		const QStringList& selectedResources = pTreeWidget->GetSelectedIds();
		if (selectedResources.isEmpty())
		{
		  reject();
		  return;
		}

		ResourceSelected(selectedResources[0].toStdString().c_str());
	});

	QPushButton* pCancelButton = new QPushButton(this);
	pCancelButton->setText(tr("Cancel"));
	pCancelButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
	connect(pCancelButton, &QPushButton::clicked, [this]()
	{
		reject();
	});

	QHBoxLayout* pActionLayout = new QHBoxLayout();
	pActionLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
	pActionLayout->addWidget(pOkButton, 0, Qt::AlignRight);
	pActionLayout->addWidget(pCancelButton, 0, Qt::AlignRight);

	m_pPreview = new QLabel();
	m_pPreview->setMinimumHeight(64);
	pMainLayout->addWidget(pTreeWidget);
	pMainLayout->addWidget(m_pPreview);
	pMainLayout->addLayout(pActionLayout);
	pMainLayout->setAlignment(m_pPreview, Qt::AlignHCenter | Qt::AlignTop);
	setLayout(pMainLayout);
}

void QResourceBrowserDialog::PreviewResource(const char* szIconPath)
{
	m_pPreview->setPixmap(QPixmap(szIconPath));
}

void QResourceBrowserDialog::ResourceSelected(const char* szIconPath)
{
	m_SelectedResource = szIconPath;
	accept();
}


