// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma optimize("",off)

// Copyright 2001-2015 Crytek GmbH. All rights reserved.
#include <StdAfx.h>
#include "GenerateHeightmapUi.h"

#include "Controls/QMenuComboBox.h"

#include <QLabel>
#include <QFormLayout>

namespace Private_GenerateHeightmapUi
{
namespace
{

const int kHeightmapResolutionStart = 1 << 7;
const int kHeightmapResolutionMax = 1 << 13;
const int kHeightmapResolutionDefault = 1 << 10;
const int kTerrainSizeMax = 1 << 13; // in meters
const float kHeightmapUnitSizeMin = 0.25f;
const float kHeightmapUnitSizeMax = 8; // limited by heightmap minimal quad tree node size, see SECTOR_SIZE_IN_UNITS
const float kHeightmapUnitsDefault = 1;

} // namespace
} // namespace Private_HeightmapDimensions

CGenerateHeightmapUi::Result::Result()
	: isTerrain(true)
	, resolution(Private_GenerateHeightmapUi::kHeightmapResolutionDefault)
	, unitSize(Private_GenerateHeightmapUi::kHeightmapUnitsDefault)
{
}

CGenerateHeightmapUi::Config::Config()
	: isOptional(true)
{
}

CGenerateHeightmapUi::CGenerateHeightmapUi(const Config& config, QFormLayout* pFormLayout, QObject* pParent)
	: QObject(pParent)
	, m_isTerrainOptional(config.isOptional)
{
	CRY_ASSERT(pFormLayout);
	if (!m_isTerrainOptional)
	{
		CRY_ASSERT(config.initial.isTerrain);
	}
	auto pResolutionComboBox = new QMenuComboBox();
	pFormLayout->addRow(tr("Heightmap &Resolution"), pResolutionComboBox);

	auto pUnitsComboBox = new QMenuComboBox();
	pFormLayout->addRow(tr("Meters per &Unit"), pUnitsComboBox);

	auto pSizeValueLabel = new QLabel();
	pFormLayout->addRow(tr("Terrain Size"), pSizeValueLabel);

	m_ui.m_pResolutionComboBox = pResolutionComboBox;
	m_ui.m_pUnitSizeComboBox = pUnitsComboBox;
	m_ui.m_pSizeValueLabel = pSizeValueLabel;

	SetupResolution(config.initial.resolution);
	SetupUnitSize(config.initial.unitSize);
	SetupTerrainSize();

	m_ui.m_pResolutionComboBox->signalCurrentIndexChanged.Connect([this](int)
	{
		IsTerrainChanged(IsTerrain());
		SetupUnitSize(GetUnitSize());
		SetupTerrainSize();
	});

	m_ui.m_pUnitSizeComboBox->signalCurrentIndexChanged.Connect([this](int)
	{
		SetupTerrainSize();
	});
}

CGenerateHeightmapUi::Result CGenerateHeightmapUi::GetResult() const
{
	Result result;
	result.isTerrain = IsTerrain();
	result.resolution = GetResolution();
	result.unitSize = GetUnitSize();
	return result;
}

bool CGenerateHeightmapUi::IsTerrain() const
{
	if (m_isTerrainOptional)
	{
		auto resolution = GetResolution();
		return 0 != resolution;
	}
	return true;
}

int CGenerateHeightmapUi::GetResolution() const
{
	auto combobox = m_ui.m_pResolutionComboBox;
	auto resolution = combobox->GetCurrentData().toInt();
	return resolution;
}

float CGenerateHeightmapUi::GetUnitSize() const
{
	auto combobox = m_ui.m_pUnitSizeComboBox;
	auto units = combobox->GetCurrentData().toFloat();
	return units;
}

void CGenerateHeightmapUi::SetupResolution(int initialResolution)
{
	auto combobox = m_ui.m_pResolutionComboBox;
	combobox->Clear();
	if (m_isTerrainOptional)
	{
		combobox->AddItem(tr("None"), (int)0);
	}
	for (auto resolution = Private_GenerateHeightmapUi::kHeightmapResolutionStart; resolution <= Private_GenerateHeightmapUi::kHeightmapResolutionMax; resolution *= 2)
	{
		auto label = QStringLiteral("%1 x %1").arg(resolution);
		combobox->AddItem(label, resolution);
		if (resolution == initialResolution)
		{
			combobox->SetChecked(combobox->GetItemCount() - 1);
		}
	}
}

void CGenerateHeightmapUi::SetupUnitSize(float initialUnitSize)
{
	auto combobox = m_ui.m_pUnitSizeComboBox;
	auto resolution = GetResolution();
	combobox->Clear();
	if (0 >= resolution)
	{
		combobox->setDisabled(true);
		return; // no unit possible
	}
	combobox->setEnabled(true);
	float unitSize = Private_GenerateHeightmapUi::kHeightmapUnitSizeMin;
	float maxUnitSize = float(Private_GenerateHeightmapUi::kTerrainSizeMax) / float(resolution);
	
	for (int i = 0; unitSize <= maxUnitSize && unitSize <= Private_GenerateHeightmapUi::kHeightmapUnitSizeMax; ++i)
	{
		auto label = QStringLiteral("%1").arg(unitSize);
		combobox->AddItem(label, unitSize);
		if (unitSize == initialUnitSize)
		{
			combobox->SetChecked(combobox->GetItemCount() - 1);
		}
		unitSize *= 2;
	}
}

void CGenerateHeightmapUi::SetupTerrainSize()
{
	auto resolution = GetResolution();
	auto unit = GetUnitSize();
	auto labelText = tr("None");
	if (0 < resolution)
	{
		auto size = resolution * unit;
		labelText = tr("%1 x %1 Meters").arg(size);
	}
	auto label = m_ui.m_pSizeValueLabel;
	label->setText(labelText);
}

