// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainTextureDimensionsUi.h"

#include "Controls/QMenuComboBox.h"

#include <QFormLayout>

namespace Private_GenerateTerrainTextureUi
{
namespace
{

static const int kResolutionStart = 1 << 9;
static const int kResolutionMax = 1 << 14;
static const int kResolutionDefault = 1 << 12;

} // namespace
} // namespace Private_TerrainTextureDimensions

CTerrainTextureDimensionsUi::Result::Result()
	: resolution(Private_GenerateTerrainTextureUi::kResolutionDefault)
{
}

CTerrainTextureDimensionsUi::CTerrainTextureDimensionsUi(const CTerrainTextureDimensionsUi::Result& initial, QFormLayout* pFormLayout, QObject* pParent)
	: QObject(pParent)
{
	auto pTextureResolutionComboBox = new QMenuComboBox();
	pFormLayout->addRow(tr("Texture &Dimensions"), pTextureResolutionComboBox);

	m_ui.m_pResolutionComboBox = pTextureResolutionComboBox;

	SetupResolutions(initial.resolution);
}

CTerrainTextureDimensionsUi::Result CTerrainTextureDimensionsUi::GetResult() const
{
	Result result;
	result.resolution = GetResolution();
	return result;
}

void CTerrainTextureDimensionsUi::SetEnabled(bool enabled)
{
	m_ui.m_pResolutionComboBox->setEnabled(enabled);
}

int CTerrainTextureDimensionsUi::GetResolution() const
{
	auto combobox = m_ui.m_pResolutionComboBox;
	return combobox->GetCurrentData().toInt();
}

void CTerrainTextureDimensionsUi::SetupResolutions(int initialResolution)
{
	auto combobox = m_ui.m_pResolutionComboBox;
	combobox->Clear();
	for (auto resolution = Private_GenerateTerrainTextureUi::kResolutionStart;
	     resolution <= Private_GenerateTerrainTextureUi::kResolutionMax;
	     resolution *= 2)
	{
		auto label = QStringLiteral("%1 x %1").arg(resolution);
		combobox->AddItem(label, resolution);
		if (resolution == initialResolution)
		{
			combobox->SetChecked(combobox->GetItemCount() - 1);
		}
	}
}

