// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>

class QSlider;
class QSpinBox;

class QSunSettingsWidget : public QWidget
{
	Q_OBJECT
public:
	QSunSettingsWidget();
	~QSunSettingsWidget();

	void OnNewScene();

protected:
	void CreateUi();
	void UpdateLightingSettings();

protected:
	QSlider*  m_latitudeSlider;
	QSpinBox* m_latitudeSpin;
	QSlider*  m_longitudeSlider;
	QSpinBox* m_longitudeSpin;
};

