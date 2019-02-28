// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OutputsWidget.h"
#include "OutputWidget.h"
#include "SubstanceCommon.h"
#include <QSizePolicy>
#include <CryIcon.h>
#include <QHBoxLayout>

#include "Controls/QMenuComboBox.h"

namespace EditorSubstance
{
	OutputsWidget::OutputsWidget(CAsset* asset, const string& graphName, std::vector<SSubstanceOutput>& outputs, QWidget* parent, bool showGlobalResolution)
		: QWidget(parent)
		, m_asset(asset)
		, m_graphName(graphName)
		, m_outputs(outputs)
	{
		setFocusPolicy(Qt::StrongFocus);
		QVBoxLayout* layout = new QVBoxLayout(this);

		if (showGlobalResolution)
		{
			QHBoxLayout* resolutionLayout = new QHBoxLayout(this);

			m_pComboXRes = new QMenuComboBox(this);
			m_pComboYRes = new QMenuComboBox(this);
			m_pResUnified = new QToolButton(this);
			m_pResUnified->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));
			m_pResUnified->setIcon(CryIcon("icons:General/Uniform.ico"));
			m_pResUnified->setIconSize(QSize(24, 24));
			m_pResUnified->setCheckable(true);
			m_pResUnified->setChecked(true);

			resolutionLayout->addWidget(m_pComboXRes);
			resolutionLayout->addWidget(m_pResUnified);
			resolutionLayout->addWidget(m_pComboYRes);

			for (int i = 2; i < 14; i++)
			{
				QVariant data(i);
				std::string text = std::to_string(int(pow(2, i)));
				m_pComboXRes->AddItem(text.c_str(), data);
				m_pComboYRes->AddItem(text.c_str(), data);
			}

			// TODO default resolution should be configurable
			m_pComboXRes->SetText("1024");
			m_pComboYRes->SetText("1024");

			layout->addLayout(resolutionLayout);

			QObject::connect(m_pResUnified, &QToolButton::clicked, this, &OutputsWidget::UniformResolutionClicked);
			m_pComboYRes->signalCurrentIndexChanged.Connect([this](int index)
			{
				ResolutionChanged(m_pComboYRes, index);
			});
			m_pComboXRes->signalCurrentIndexChanged.Connect([this](int index)
			{
				ResolutionChanged(m_pComboXRes, index);
			});
		}


		m_pEditOutputs = new QPushButton("Edit Outputs");
		layout->addWidget(m_pEditOutputs);

		QCheckBox* showAll = new QCheckBox("Show All Presets", this);
		showAll->setChecked(false);
		layout->addWidget(showAll);

		connect(showAll, &QCheckBox::stateChanged, this, &OutputsWidget::ShowAllPresets);

		updateOutputsWidgets();
		setLayout(layout);




	}

	void OutputsWidget::SetResolution(const Vec2i& resolution)
	{
		m_pComboYRes->blockSignals(true);
		m_pComboXRes->blockSignals(true);
		m_pComboXRes->SetText(std::to_string(int(pow(2, resolution.x))).c_str());
		m_pComboYRes->SetText(std::to_string(int(pow(2, resolution.y))).c_str());
		m_pResUnified->setChecked(resolution.x == resolution.y);
		
		m_pComboYRes->blockSignals(false);
		m_pComboXRes->blockSignals(false);
	}

	void OutputsWidget::RefreshOutputs()
	{
		QList<OutputWidget*> outputs = findChildren<OutputWidget*>(QString(), Qt::FindDirectChildrenOnly);
		for (auto child : outputs)
		{
			layout()->removeWidget(child);
			delete child;
		}

		updateOutputsWidgets();
	}

	void OutputsWidget::updateOutputsWidgets()
	{
		for (SSubstanceOutput& output : m_outputs)
		{
			if (!output.IsValid())
			{
				continue;
			}
			OutputWidget* outputWidget = new OutputWidget(&output, this);
			QObject::connect(outputWidget, &OutputWidget::outputChanged, [=]() { outputChanged(outputWidget->GetOutput()); });
			QObject::connect(outputWidget, &OutputWidget::outputStateChanged, [=]() { outputStateChanged(outputWidget->GetOutput()); });
			layout()->addWidget(outputWidget);
		}
	}

	void OutputsWidget::UniformResolutionClicked()
	{
		if (m_pResUnified->isChecked())
		{
			m_pComboYRes->SetText(m_pComboXRes->GetText());
		}
	}

	void OutputsWidget::ResolutionChanged(QMenuComboBox* sender, int index)
	{
		if (!m_pResUnified->isChecked())
			return;

		if (sender == m_pComboXRes && m_pComboYRes->GetText() != sender->GetText())
		{
			m_pComboYRes->SetText(sender->GetText());
		}

		if (sender == m_pComboYRes && m_pComboXRes->GetText() != sender->GetText())
		{
			m_pComboXRes->SetText(sender->GetText());
		}

		onResolutionChanged(Vec2i(m_pComboXRes->GetCurrentData().toInt(), m_pComboYRes->GetCurrentData().toInt()));
	}

	void OutputsWidget::ShowAllPresets(bool state)
	{
		QList<OutputWidget*> outputs = findChildren<OutputWidget*>(QString(), Qt::FindDirectChildrenOnly);
		for (auto widget : outputs)
		{
			widget->SetShowAllPresets(state);
		}
	}

}
