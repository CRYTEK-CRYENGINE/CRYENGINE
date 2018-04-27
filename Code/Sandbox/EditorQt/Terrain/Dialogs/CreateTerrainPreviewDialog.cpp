// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CreateTerrainPreviewDialog.h"

#include <QSpacerItem>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QFormLayout>

CCreateTerrainPreviewDialog::CCreateTerrainPreviewDialog(const Result& initial)
	: CEditorDialog(QStringLiteral("CreateTerrainPreviewDialog"))
	, m_pDimensions(nullptr)
{
	setWindowTitle(tr("Create Terrain Preview"));

	auto pFormLayout = new QFormLayout();

	m_pDimensions = new CTerrainTextureDimensionsUi(initial, pFormLayout, this);

	auto pVerticalSpacer = new QSpacerItem(20, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);

	auto pDialogButtonBox = new QDialogButtonBox(this);
	pDialogButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	auto pDialogLayout = new QVBoxLayout(this);
	pDialogLayout->addLayout(pFormLayout);
	pDialogLayout->addItem(pVerticalSpacer);
	pDialogLayout->addWidget(pDialogButtonBox);

	connect(pDialogButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(pDialogButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

CCreateTerrainPreviewDialog::Result CCreateTerrainPreviewDialog::GetResult() const
{
	return m_pDimensions->GetResult();
}

