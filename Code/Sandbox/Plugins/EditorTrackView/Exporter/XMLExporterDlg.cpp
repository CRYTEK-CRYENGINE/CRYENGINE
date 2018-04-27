// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "Stdafx.h"
#include "XMLExporterDlg.h"

#include <QFormLayout>
#include <QLabel>
#include <QCheckBox>
#include <QDialogButtonBox>

QXMLExporterDlg::QXMLExporterDlg()
	: CEditorDialog("XML Exporter")
	, m_bConfirmed(false)
{
	QVBoxLayout* pDialogLayout = new QVBoxLayout(this);
	QFormLayout* pFormLayout = new QFormLayout();
	pDialogLayout->addLayout(pFormLayout);

	m_pLabel = new QLabel();
	m_pLabel->setText(QObject::tr("Export Keys in Tracks"));
	pFormLayout->addRow(m_pLabel);

	m_pExportAnimation = new QCheckBox();
	m_pExportAnimation->setText(QObject::tr("Animation"));
	pFormLayout->addRow(m_pExportAnimation);

	m_pExportAudio = new QCheckBox();
	m_pExportAudio->setText(QObject::tr("Audio"));
	pFormLayout->addRow(m_pExportAudio);

	QDialogButtonBox* pButtonBox = new QDialogButtonBox(this);
	pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	pDialogLayout->addWidget(pButtonBox);
	QObject::connect(pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	QObject::connect(pButtonBox, &QDialogButtonBox::accepted, this, &QXMLExporterDlg::OnConfirmed);
}

bool QXMLExporterDlg::GetResult(bool& bExportAnimation, bool& bExportAudio) const
{
	if (m_bConfirmed)
	{
		bExportAnimation = m_pExportAnimation->isChecked();
		bExportAudio = m_pExportAudio->isChecked();
		return true;
	}
	return false;
}

void QXMLExporterDlg::OnConfirmed()
{
	m_bConfirmed = true;
	QDialog::accept();
}

