// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <QString>
#include <QCheckBox>
#include <QLabel>

struct SSubstanceOutput;
class QMenuComboBox;

namespace EditorSubstance
{
class OutputWidget: public QWidget
{
	Q_OBJECT;
public:
	OutputWidget(SSubstanceOutput* output, QWidget* parent= nullptr);
	void SetShowAllPresets(bool state);
	SSubstanceOutput* GetOutput() { return m_output; }
signals:	
	void outputStateChanged();
	void outputChanged();
protected:
	void UpdateElements();
private:
	SSubstanceOutput* m_output;
	QMenuComboBox* m_preset;
	QMenuComboBox* m_resolution;
	QLabel* m_name;
	QCheckBox* m_enabled;
};

}
