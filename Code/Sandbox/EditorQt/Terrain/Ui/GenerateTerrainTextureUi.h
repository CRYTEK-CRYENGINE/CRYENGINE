// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QObject>

class QFormLayout;
class CTerrainTextureDimensionsUi;
class QNumericBox;
class QCheckBox;

class CGenerateTerrainTextureUi
	: public QObject
{
	Q_OBJECT
public:
	struct Result
	{
		Result();
		int   resolution;
		float colorMultiplier;

		bool  isHighQualityCompression;
	};

public:
	explicit CGenerateTerrainTextureUi(const Result& initial, QFormLayout* pFormLayout, QObject* parent = nullptr);

	Result GetResult() const;

	void   SetEnabled(bool enabled);

private:
	int   GetResolution() const;
	float GetColorMultiplier() const;
	bool  IsHighQualityCompression() const;

private:
	struct Ui
	{
		CTerrainTextureDimensionsUi* m_pTerrainTextureDimensionsUi;
		QNumericBox*           m_pColorMultiplierSpinBox;
		QCheckBox*                   m_pHighQualityCompressionCheckBox;
	};
	Ui m_ui;
};

