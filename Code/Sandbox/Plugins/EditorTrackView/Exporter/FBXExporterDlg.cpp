// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "Stdafx.h"
#include "FBXExporterDlg.h"
#include "Controls/QMenuComboBox.h"

#include <CryMovie/AnimTime.h>
#include <QFormLayout>
#include <QCheckBox>
#include <QDialogButtonBox>

QFBXExporterDlg::QFBXExporterDlg(bool bDisplayFPSSettingOnly, bool bExportMasterCamOnly, bool bExportLocalToSelected)
	: CEditorDialog("FBX Exporter")
	, m_bConfirmed(false)
{
	QVBoxLayout* pDialogLayout = new QVBoxLayout(this);
	QFormLayout* pFormLayout = new QFormLayout();
	pDialogLayout->addLayout(pFormLayout);

	m_pFPS = new QMenuComboBox();
	m_pFPS->SetMultiSelect(false);
	m_pFPS->SetCanHaveEmptySelection(false);
	for (uint i = 0; i < SAnimTime::eFrameRate_Num; ++i)
	{
		SAnimTime::EFrameRate frameRate = static_cast<SAnimTime::EFrameRate>(i);
		const char* szFrameRateName = SAnimTime::GetFrameRateName(frameRate);
		uint frameRateValue = SAnimTime::GetFrameRateValue(frameRate);
		m_pFPS->AddItem(szFrameRateName, QVariant::fromValue(frameRateValue));
	}
	pFormLayout->addRow(QObject::tr("&Select Bake FPS:"), m_pFPS);

	m_pExportOnlyMasterCamera = new QCheckBox();
	m_pExportOnlyMasterCamera->setText(QObject::tr("&Export Master Camera only"));
	m_pExportOnlyMasterCamera->setChecked(bExportMasterCamOnly);
	m_pExportOnlyMasterCamera->setEnabled(!bDisplayFPSSettingOnly);
	pFormLayout->addRow(m_pExportOnlyMasterCamera);

	m_pExportAsLocalToSelected = new QCheckBox();
	m_pExportAsLocalToSelected->setText(QObject::tr("&Export as Local to the selected Object"));
	m_pExportAsLocalToSelected->setChecked(bExportLocalToSelected);
	m_pExportAsLocalToSelected->setEnabled(!bDisplayFPSSettingOnly);
	pFormLayout->addRow(m_pExportAsLocalToSelected);

	QDialogButtonBox* pButtonBox = new QDialogButtonBox(this);
	pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	pDialogLayout->addWidget(pButtonBox);
	QObject::connect(pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	QObject::connect(pButtonBox, &QDialogButtonBox::accepted, this, &QFBXExporterDlg::OnConfirmed);
}

bool QFBXExporterDlg::GetResult(uint& frameRate, bool& bExportMasterCamOnly, bool& bExportLocalToSelected) const
{
	if (m_bConfirmed)
	{
		const auto currentIndex = m_pFPS->GetCheckedItem();
		frameRate = m_pFPS->GetData(currentIndex).value<uint>();
		bExportMasterCamOnly = m_pExportOnlyMasterCamera->isChecked();
		bExportLocalToSelected = m_pExportAsLocalToSelected->isChecked();
		return true;
	}
	return false;
}

void QFBXExporterDlg::OnConfirmed()
{
	m_bConfirmed = true;
	QDialog::accept();
}

