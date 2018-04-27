// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <QString>
#include <QCheckBox>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>

struct SSubstanceOutput;
class CAsset;
class QMenuComboBox;


namespace EditorSubstance
{
	class OutputWidget;

	class OutputsWidget : public QWidget
	{
		Q_OBJECT;
	public:
		OutputsWidget(CAsset* asset, const string& graphName, std::vector<SSubstanceOutput>& outputs, QWidget* parent = nullptr, bool showGlobalResolution = true);
		QPushButton* GetEditOutputsButton() { return m_pEditOutputs; }
		void SetResolution(const Vec2i& resolution);
		void RefreshOutputs();

	signals:
		void outputChanged(SSubstanceOutput* output);
		void outputStateChanged(SSubstanceOutput* output);
		void onResolutionChanged(const Vec2i& resolution);
	protected:
		void updateOutputsWidgets();
		protected slots :
		void UniformResolutionClicked();
		void ResolutionChanged(QMenuComboBox* sender, int index);
		void ShowAllPresets(bool state);
	private:
		CAsset* m_asset;
		string m_graphName;
		std::vector<SSubstanceOutput>& m_outputs;
		QMenuComboBox* m_pComboXRes;
		QMenuComboBox* m_pComboYRes;
		QToolButton* m_pResUnified;
		QPushButton* m_pEditOutputs;
		Vec2i m_resolution;
	};
}
