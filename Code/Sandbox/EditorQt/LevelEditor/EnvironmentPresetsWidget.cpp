// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EnvironmentPresetsWidget.h"

#include "IEditorImpl.h"

#include <AssetSystem/Asset.h>
#include <AssetSystem/Browser/AssetBrowserDialog.h>
#include <IUndoObject.h>
#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>

#include <Cry3DEngine/I3DEngine.h>
#include <CrySerialization/Decorators/Resources.h>

#include <QBoxLayout>

namespace Private_EnvironmentPresetWidget
{

ITimeOfDay* GetTimeOfDay()
{
	return GetIEditorImpl()->Get3DEngine()->GetTimeOfDay();
}

struct SPreset
{
	void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::MakeResourceSelector<>(name, "Environment"), "name", "^Name");
	}

	friend bool operator<(const SPreset& a, const SPreset& b)
	{
		return a.name.CompareNoCase(b.name) < 0;
	}

	friend bool operator==(const SPreset& a, const SPreset& b)
	{
		return a.name.CompareNoCase(b.name) == 0;
	}

	string name;
	bool   isDefault = false;
};

class SPresetsSerializer : public CEnvironmentPresetsWidget::IPresetsSerializer
{
public:
	void Init() override
	{
		const ITimeOfDay* const pTimeOfDay = GetTimeOfDay();
		const int presetCount = pTimeOfDay->GetPresetCount();
		std::vector<ITimeOfDay::SPresetInfo> presetInfos(presetCount);
		pTimeOfDay->GetPresetsInfos(presetInfos.data(), presetCount);

		m_presets.resize(presetInfos.size());
		for (size_t i = 0, n = presetInfos.size(); i < n; ++i)
		{
			m_presets[i].name = presetInfos[i].szName;
			m_presets[i].isDefault = presetInfos[i].isDefault;
		}
	}

	void Serialize(Serialization::IArchive& ar) override
	{
		std::vector<SPreset> oldValues;
		if (ar.isInput())
		{
			oldValues = m_presets;
		}

		string defaultPreset = GetDefault();
		Serialization::StringList items;
		for (const SPreset& preset : m_presets)
		{
			items.push_back(preset.name);
		}

		if (!items.empty())
		{
			if (defaultPreset.empty())
			{
				defaultPreset = items.front();
			}

			Serialization::StringListValue dropDown(items, defaultPreset);
			ar(dropDown, "default", "Default Preset");

			if (ar.isInput())
			{
				SetDefault(dropDown.c_str());
			}
		}

		ar(m_presets, "presets", "Presets");

		if (ar.isInput())
		{
			UpdateTimeOfDay(oldValues, std::vector<SPreset>(m_presets));
		}
	}

	void Apply()
	{
		SPresetsSerializer current;
		current.Init();
		UpdateTimeOfDay(current.m_presets, std::vector<SPreset>(m_presets));
	}

private:
	string GetDefault() const
	{
		auto it = std::find_if(m_presets.cbegin(), m_presets.cend(), [](const SPreset& x)
		{
			return x.isDefault;
		});

		if (it != m_presets.cend())
		{
			return it->name;
		}

		return string();
	}

	void SetDefault(const string& defaultPreset)
	{
		for (SPreset& preset : m_presets)
		{
			preset.isDefault = preset.name.CompareNoCase(defaultPreset) == 0;
		}
	}

	void UpdateTimeOfDay(std::vector<SPreset>& oldValues, std::vector<SPreset>& newValues)
	{
		std::sort(oldValues.begin(), oldValues.end());
		std::sort(newValues.begin(), newValues.end());
		std::unique(newValues.begin(), newValues.end());

		std::vector<SPreset> difference;

		// Handle deleted
		std::set_difference(oldValues.cbegin(), oldValues.cend(), newValues.cbegin(), newValues.cend(), std::back_inserter(difference));
		if (!difference.empty())
		{
			ITimeOfDay* const pTimeOfDay = GetTimeOfDay();
			for (const SPreset& preset : difference)
			{
				pTimeOfDay->RemovePreset(preset.name);
			}
		}

		// Handle added
		difference.clear();
		std::set_difference(newValues.cbegin(), newValues.cend(), oldValues.cbegin(), oldValues.cend(), std::back_inserter(difference));
		if (!difference.empty())
		{
			ITimeOfDay* const pTimeOfDay = GetTimeOfDay();
			for (const SPreset& preset : difference)
			{
				if (!preset.name.IsEmpty())
				{
					pTimeOfDay->LoadPreset(preset.name);
					continue;
				}
			}
		}

		// Set new default preset
		for (const SPreset& preset : newValues)
		{
			if (!preset.isDefault)
			{
				continue;
			}

			ITimeOfDay* const pTimeOfDay = GetTimeOfDay();
			pTimeOfDay->SetDefaultPreset(preset.name);
			break;
		}
	}
private:
	std::vector<SPreset> m_presets;
	string m_default;
};

class CUndoLevelPresets : public IUndoObject
{
public:
	CUndoLevelPresets()
	{
		m_undo.Init();
	}

protected:
	void Undo(bool bUndo)
	{
		if (bUndo)
		{
			m_redo.Init();
		}
		m_undo.Apply();
	}

	void Redo()
	{
		m_redo.Apply();
	}

	const char* GetDescription() { return "Modify Level Presets"; }
private:
	SPresetsSerializer m_undo;
	SPresetsSerializer m_redo;
};

}

CEnvironmentPresetsWidget::CEnvironmentPresetsWidget(QWidget* pParent /*= nullptr*/)
	: QWidget(pParent)
{
	using namespace Private_EnvironmentPresetWidget;

	m_pPropertyTree = new QPropertyTreeLegacy(this);
	
	connect(m_pPropertyTree, &QPropertyTreeLegacy::signalBeginUndo, this, &CEnvironmentPresetsWidget::OnBeginUndo);
	connect(m_pPropertyTree, &QPropertyTreeLegacy::signalEndUndo, this, &CEnvironmentPresetsWidget::OnEndUndo);
	connect(m_pPropertyTree, &QPropertyTreeLegacy::signalAboutToSerialize, this, &CEnvironmentPresetsWidget::BeforeSerialization);
	connect(m_pPropertyTree, &QPropertyTreeLegacy::signalSerialized, this, &CEnvironmentPresetsWidget::AfterSerialization);
	setAttribute(Qt::WA_DeleteOnClose);

	m_pPropertyTree->setSizeHint(QSize(70, 70));
	m_pPropertyTree->setValueColumnWidth(0.70f);
	m_pPropertyTree->setExpandLevels(2);
	m_pPropertyTree->setShowContainerIndices(false);

	m_pPresetsSerializer.reset(new SPresetsSerializer());
	m_pPresetsSerializer->Init();
	m_pPropertyTree->attach(Serialization::SStruct(*m_pPresetsSerializer.get()));

	setLayout(new QVBoxLayout());
	layout()->setContentsMargins(0, 0, 0, 0);
	layout()->addWidget(m_pPropertyTree);

	GetTimeOfDay()->RegisterListener(this);
}

CEnvironmentPresetsWidget::~CEnvironmentPresetsWidget()
{
	Private_EnvironmentPresetWidget::GetTimeOfDay()->UnRegisterListener(this);
}

void CEnvironmentPresetsWidget::OnBeginUndo()
{
	GetIEditor()->GetIUndoManager()->Begin();
	GetIEditor()->GetIUndoManager()->RecordUndo(new Private_EnvironmentPresetWidget::CUndoLevelPresets());
}

void CEnvironmentPresetsWidget::OnEndUndo(bool acceptUndo)
{
	if (acceptUndo)
	{
		GetIEditor()->GetIUndoManager()->Accept("Modify level presets");
	}
	else
	{
		GetIEditor()->GetIUndoManager()->Cancel();
	}
}

void CEnvironmentPresetsWidget::BeforeSerialization(Serialization::IArchive& ar)
{
	m_ignoreEvent = true;
}

void CEnvironmentPresetsWidget::AfterSerialization(Serialization::IArchive& ar)
{
	m_ignoreEvent = false;
}

void CEnvironmentPresetsWidget::OnChange(const ITimeOfDay::IListener::EChangeType changeType, const char* const szPresetName)
{
	if (m_ignoreEvent)
	{
		return;
	}

	m_pPresetsSerializer->Init();
	m_pPropertyTree->revert();
}
