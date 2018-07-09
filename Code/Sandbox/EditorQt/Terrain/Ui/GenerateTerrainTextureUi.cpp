// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "GenerateTerrainTextureUi.h"

#include "TerrainTextureDimensionsUi.h"

#include <QFormLayout>
#include <QCheckBox>
#include "Controls/QNumericBox.h"

CGenerateTerrainTextureUi::Result::Result()
	: resolution(CTerrainTextureDimensionsUi::Result().resolution)
	, colorMultiplier(8)
	, isHighQualityCompression(true)
{
}

CGenerateTerrainTextureUi::CGenerateTerrainTextureUi(const Result& initial, QFormLayout* pFormLayout, QObject* parent)
	: QObject(parent)
{
	auto resolutionInitial = CTerrainTextureDimensionsUi::Result();
	resolutionInitial.resolution = initial.resolution;
	auto pTerrainTextureDimensionsUi = new CTerrainTextureDimensionsUi(resolutionInitial, pFormLayout, parent);

	auto pTextureColorMultiplierSpinBox = new QNumericBox();
	pTextureColorMultiplierSpinBox->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	pTextureColorMultiplierSpinBox->setAccelerated(true);
	pTextureColorMultiplierSpinBox->setMinimum(1);
	pTextureColorMultiplierSpinBox->setMaximum(16);
	pTextureColorMultiplierSpinBox->setSingleStep(0.01);
	pTextureColorMultiplierSpinBox->setValue(initial.colorMultiplier);
	pFormLayout->addRow(tr("Terrain Color &Multiplier"), pTextureColorMultiplierSpinBox);

	auto pHighQualityCompressionCheckBox = new QCheckBox();
	pHighQualityCompressionCheckBox->setText(tr("High &Quality Terrain Texture"));
	pHighQualityCompressionCheckBox->setChecked(initial.isHighQualityCompression);
	pFormLayout->addRow(pHighQualityCompressionCheckBox);

	m_ui.m_pTerrainTextureDimensionsUi = pTerrainTextureDimensionsUi;
	m_ui.m_pColorMultiplierSpinBox = pTextureColorMultiplierSpinBox;
	m_ui.m_pHighQualityCompressionCheckBox = pHighQualityCompressionCheckBox;
}

CGenerateTerrainTextureUi::Result CGenerateTerrainTextureUi::GetResult() const
{
	Result result;
	result.resolution = GetResolution();
	result.colorMultiplier = GetColorMultiplier();
	result.isHighQualityCompression = IsHighQualityCompression();
	return result;
}

void CGenerateTerrainTextureUi::SetEnabled(bool enabled)
{
	m_ui.m_pTerrainTextureDimensionsUi->SetEnabled(enabled);
	m_ui.m_pColorMultiplierSpinBox->setEnabled(enabled);
	m_ui.m_pHighQualityCompressionCheckBox->setEnabled(enabled);
}

int CGenerateTerrainTextureUi::GetResolution() const
{
	auto ui = m_ui.m_pTerrainTextureDimensionsUi;
	return ui->GetResult().resolution;
}

float CGenerateTerrainTextureUi::GetColorMultiplier() const
{
	auto doubleSpinBox = m_ui.m_pColorMultiplierSpinBox;
	return doubleSpinBox->value();
}

bool CGenerateTerrainTextureUi::IsHighQualityCompression() const
{
	auto checkbox = m_ui.m_pHighQualityCompressionCheckBox;
	return checkbox->isChecked();
}

