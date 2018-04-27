// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QObject>

class QMenuComboBox;

class QFormLayout;

/**
 * @brief Manages form elements to select a proper terrain texture size
 */
class CTerrainTextureDimensionsUi
	: public QObject
{
	Q_OBJECT

public:
	struct Result
	{
		Result();
		int resolution;
	};

public:
	explicit CTerrainTextureDimensionsUi(const Result& initial, QFormLayout* pFormLayout, QObject* pParent = nullptr);

	Result GetResult() const;

	void   SetEnabled(bool enabled);

private:
	int GetResolution() const;

private:
	void SetupResolutions(int);

private:
	struct Ui
	{
		QMenuComboBox* m_pResolutionComboBox;
	};
	Ui m_ui;
};

