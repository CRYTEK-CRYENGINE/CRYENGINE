// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "ResizeTerrainDialog.h"

#include <QFormLayout>
#include <QDialogButtonBox>

CResizeTerrainDialog::CResizeTerrainDialog(const Result& initial)
	: CEditorDialog("HeightmapResizeDialog")
	, m_pHeightmap(nullptr)
	, m_pTexture(nullptr)
{
	setWindowTitle(tr("Resize Heightmap"));

	auto pFormLayout = new QFormLayout();

	CGenerateHeightmapUi::Config heightmapConfig;
	heightmapConfig.isOptional = false;
	heightmapConfig.initial = initial.heightmap;
	m_pHeightmap = new CGenerateHeightmapUi(heightmapConfig, pFormLayout, this);

	m_pTexture = new CGenerateTerrainTextureUi(initial.texture, pFormLayout, this);

	auto verticalSpacer = new QSpacerItem(20, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);

	auto pDialogButtonBox = new QDialogButtonBox(this);
	pDialogButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	auto dialogLayout = new QVBoxLayout(this);
	dialogLayout->addLayout(pFormLayout);
	dialogLayout->addItem(verticalSpacer);
	dialogLayout->addWidget(pDialogButtonBox);

	connect(pDialogButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(pDialogButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

CResizeTerrainDialog::Result CResizeTerrainDialog::GetResult() const
{
	Result result;
	result.heightmap = m_pHeightmap->GetResult();
	result.texture = m_pTexture->GetResult();
	return result;
}

