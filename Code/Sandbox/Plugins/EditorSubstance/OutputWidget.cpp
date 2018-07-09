// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OutputWidget.h"
#include "SubstanceCommon.h"
#include "EditorSubstanceManager.h"
#include <QSizePolicy>

#include <QHBoxLayout>
#include "Controls/QMenuComboBox.h"

namespace EditorSubstance
{

	OutputWidget::OutputWidget(SSubstanceOutput* output, QWidget* parent)
		: QWidget(parent), m_output(output)
	{

		m_enabled = new QCheckBox(this);
		m_name = new QLabel(this);
		m_preset = new QMenuComboBox(this);
		m_resolution = new QMenuComboBox(this);
		m_name->setMaximumWidth(50);
		m_name->setMinimumWidth(50);
		m_name->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

		m_resolution->setMaximumWidth(100);
		m_resolution->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

		QHBoxLayout* layout = new QHBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(m_enabled);
		layout->addWidget(m_name);
		layout->addWidget(m_preset);
		layout->addWidget(m_resolution);
		setLayout(layout);

		std::set<string> availablePresets = CManager::Instance()->GetTexturePresetsForFile(m_output->name);
		for each (const string& var in availablePresets)
		{
			m_preset->AddItem(var.c_str());
		}

		for each (auto var in resolutionNamesMap)
		{
			m_resolution->AddItem(var.first.c_str(), QVariant(var.second));
		}

		connect(m_enabled, &QCheckBox::stateChanged, [this](bool state) {
			m_output->enabled = state;
			outputStateChanged();
		});

		m_resolution->signalCurrentIndexChanged.Connect([this](int index)
		{
			m_output->resolution = static_cast<SSubstanceOutput::ESubstanceOutputResolution>(m_resolution->GetData(index).toInt());
			outputChanged();
		});

		m_preset->signalCurrentIndexChanged.Connect([this](int index)
		{
			m_output->preset = m_preset->GetText().toStdString().c_str();
			outputChanged();
		});


		UpdateElements();
	}



	void OutputWidget::SetShowAllPresets(bool state)
	{
		QString currentText = m_preset->GetText();
		string currentName("");
		if (!state)
			currentName = m_output->name;

		m_preset->Clear();

		std::set<string> availablePresets = CManager::Instance()->GetTexturePresetsForFile(currentName);
		if (!state && (availablePresets.find(m_output->preset) == availablePresets.end()))
		{
			availablePresets.emplace(currentText.toStdString().c_str());
		}

		for each (const string& var in availablePresets)
		{
			m_preset->AddItem(var.c_str());
		}

		m_preset->SetText(currentText);
	}

	void OutputWidget::UpdateElements()
	{
		m_name->setText(m_output->name.c_str());
		m_enabled->setChecked(m_output->enabled);
		QString actualPresset(m_output->preset.c_str());
		m_preset->SetText(actualPresset);
		if (m_preset->GetText() != actualPresset)
		{
			m_preset->AddItem(actualPresset);
			m_preset->SetText(actualPresset);
		}
		for (size_t i = 0; i < m_resolution->GetItemCount(); i++)
		{
			if (m_resolution->GetData(i).toInt() == m_output->resolution)
			{
				m_resolution->SetText(m_resolution->GetItem(i));
				break;
			}
		}
	}

}
