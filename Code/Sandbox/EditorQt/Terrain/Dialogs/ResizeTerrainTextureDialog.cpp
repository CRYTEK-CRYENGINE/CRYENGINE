// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "ResizeTerrainTextureDialog.h"

#include <QSpacerItem>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QFormLayout>

CResizeTerrainTextureDialog::CResizeTerrainTextureDialog(const Result& initial)
	: CEditorDialog(QStringLiteral("GenerateTextureDialog"))
	, m_pTexture(nullptr)
{
	setWindowTitle(tr("Generate Terrain Texture"));

	auto pFormLayout = new QFormLayout();

	m_pTexture = new CGenerateTerrainTextureUi(initial, pFormLayout, this);

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

CResizeTerrainTextureDialog::Result CResizeTerrainTextureDialog::GetResult() const
{
	return m_pTexture->GetResult();
}

