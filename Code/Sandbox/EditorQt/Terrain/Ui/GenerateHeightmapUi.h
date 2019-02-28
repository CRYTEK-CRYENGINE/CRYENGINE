// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QObject>

class QMenuComboBox;

class QLabel;
class QFormLayout;

class CGenerateHeightmapUi
	: public QObject
{
	Q_OBJECT

public:
	struct Result
	{
		Result();
		bool isTerrain;
		int  resolution;
		float unitSize;
	};
	struct Config
	{
		Config();
		bool   isOptional;
		Result initial;
	};

public:
	CGenerateHeightmapUi(const Config& config, QFormLayout* pFormLayout, QObject* pParent);

	Result GetResult() const;

signals:
	void IsTerrainChanged(bool);

private:
	bool IsTerrain() const;
	int  GetResolution() const;
	float GetUnitSize() const;

	void SetupResolution(int initialResolution);
	void SetupUnitSize(float initialUnitSize);
	void SetupTerrainSize();

private:
	const bool m_isTerrainOptional;

	struct Ui
	{
		QMenuComboBox* m_pResolutionComboBox;
		QMenuComboBox* m_pUnitSizeComboBox;
		QLabel*        m_pSizeValueLabel;
	};
	Ui m_ui;
};

