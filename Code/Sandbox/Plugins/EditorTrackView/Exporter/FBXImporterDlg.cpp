// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "Stdafx.h"
#include "FBXImporterDlg.h"

#include <QFormLayout>
#include <QListView>
#include <QListWidgetItem>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QDialogButtonBox>

QFBXImporterDlg::QFBXImporterDlg()
	: CEditorDialog("FBX Importer")
	, m_bConfirmed(false)
{
	QVBoxLayout* pDialogLayout = new QVBoxLayout(this);
	QFormLayout* pFormLayout = new QFormLayout();
	pDialogLayout->addLayout(pFormLayout);

	m_pObjectList = new QListView();
	m_pObjectList->setMinimumHeight(100);
	pFormLayout->addRow(m_pObjectList);

	m_pModel = new QStandardItemModel();
	m_pObjectList->setModel(m_pModel);

	QDialogButtonBox* pButtonBox = new QDialogButtonBox(this);
	pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	pDialogLayout->addWidget(pButtonBox);
	QObject::connect(pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	QObject::connect(pButtonBox, &QDialogButtonBox::accepted, this, &QFBXImporterDlg::OnConfirmed);
}

void QFBXImporterDlg::AddImportObject(string objectName)
{
	QStandardItem* pItem = new QStandardItem();
	pItem->setData(QObject::tr(objectName), Qt::DisplayRole);
	pItem->setCheckable(true);
	pItem->setCheckState(Qt::Unchecked);
	m_pModel->appendRow(pItem);
}

bool QFBXImporterDlg::IsObjectSelected(string objectName)
{
	int objectCount = m_pModel->rowCount();
	for (uint i = 0; i < objectCount; ++i)
	{
		QStandardItem* pItem = m_pModel->item(i);
		QString listObjectName = pItem->data(Qt::DisplayRole).toString();

		if (!stricmp(listObjectName.toUtf8().data(), objectName))
		{
			return (Qt::Checked == pItem->checkState());
		}
	}
	return false;
}

void QFBXImporterDlg::OnConfirmed()
{
	m_bConfirmed = true;
	QDialog::accept();
}

