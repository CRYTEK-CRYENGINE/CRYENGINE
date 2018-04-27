// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "AnimEventPresetPanel.h"
#include "AnimEvent.h"
#include "BlockPalette.h"
#include "BlockPaletteContent.h"
#include <QBoxLayout>
#include <QDir>
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include "QPropertyTree/ContextList.h"
#include "Serialization.h"
#include "Expected.h"
#include "CharacterDocument.h"
#include "CharacterToolSystem.h"
#include <IEditor.h>
#include <CrySerialization/IArchiveHost.h>

namespace CharacterTool
{

struct AnimEventPresetCollection
{
	AnimEventPresets presets;

	void             Serialize(Serialization::IArchive& ar)
	{
		ar(presets, "presets", "Presets");
	}
};

AnimEventPresetPanel::AnimEventPresetPanel(QWidget* parent, System* system)
	: QWidget(parent)
	, m_presetCollection(new AnimEventPresetCollection)
{
	QBoxLayout* layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
	m_palette = new BlockPalette(this);
	EXPECTED(connect(m_palette, SIGNAL(SignalSelectionChanged()), this, SLOT(OnPaletteSelectionChanged())));
	EXPECTED(connect(m_palette, SIGNAL(SignalItemClicked(const BlockPaletteItem &)), this, SLOT(OnPaletteItemClicked(const BlockPaletteItem &))));
	EXPECTED(connect(m_palette, SIGNAL(SignalChanged()), this, SLOT(OnPaletteChanged())));
	layout->addWidget(m_palette);

	m_tree = new QPropertyTree(this);
	PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	treeStyle.groupRectangle = false;
	m_tree->setTreeStyle(treeStyle);
	m_tree->setSizeToContent(true);
	m_tree->setCompact(true);
	m_tree->setArchiveContext(system->contextList->Tail());
	EXPECTED(connect(m_tree, SIGNAL(signalChanged()), this, SLOT(OnPropertyTreeChanged())));
	EXPECTED(connect(m_tree, SIGNAL(signalContinuousChange()), this, SLOT(OnPropertyTreeChanged())));
	layout->addWidget(m_tree);

	WritePalette();
}

AnimEventPresetPanel::~AnimEventPresetPanel()
{
}

static QColor PresetColor(float hue)
{
	return QColor::fromHslF(hue, 0.9f, 0.85f);
}

void AnimEventPresetPanel::WritePalette()
{
	BlockPaletteContent content;
	content.items.resize(m_presetCollection->presets.size());
	for (size_t i = 0; i < m_presetCollection->presets.size(); ++i)
	{
		const AnimEventPreset& preset = m_presetCollection->presets[i];
		BlockPaletteItem& item = content.items[i];
		item.userId = i;
		item.name = preset.name;
		QColor color = PresetColor(preset.colorHue);
		item.color = ColorB(color.red(), color.green(), color.blue(), 255);
		SerializeToMemory(&item.userPayload, Serialization::SStruct(preset.event));
	}
	m_palette->SetContent(content);
}

void AnimEventPresetPanel::ReadPalette()
{
	const BlockPaletteItems& items = m_palette->Content().items;
	AnimEventPresets& presets = m_presetCollection->presets;
	presets.clear();

	for (size_t i = 0; i < items.size(); ++i)
	{
		const BlockPaletteItem& item = items[i];

		AnimEventPreset preset;
		SerializeFromMemory(Serialization::SStruct(preset.event), item.userPayload);

		preset.colorHue = QColor(item.color.r, item.color.g, item.color.b).hslHueF();
		if (preset.colorHue < 0.0f)
			preset.colorHue = 0.0f;
		preset.name = item.name;
		preset.event.startTime = -1.0f;
		preset.event.endTime = -1.0f;
		presets.push_back(preset);
	}
}

void AnimEventPresetPanel::OnPropertyTreeChanged()
{
	WritePalette();
	PresetsChanged();
}

void AnimEventPresetPanel::OnPaletteSelectionChanged()
{
	BlockPaletteSelectedIds ids;
	m_palette->GetSelectedIds(&ids);
	std::vector<Serialization::SStruct> structs;
	for (size_t i = 0; i < ids.size(); ++i)
		structs.push_back(Serialization::SStruct(m_presetCollection->presets[ids[i].userId]));
	m_tree->attach(structs);
}

void AnimEventPresetPanel::OnPaletteItemClicked(const BlockPaletteItem& item)
{
	if (size_t(item.userId) >= m_presetCollection->presets.size())
		return;
	SignalPutEvent(m_presetCollection->presets[item.userId]);
}

void AnimEventPresetPanel::OnPaletteChanged()
{
	ReadPalette();
	WritePalette();
	PresetsChanged();
}

const AnimEventPreset* AnimEventPresetPanel::GetPresetByHotkey(int number) const
{
	const BlockPaletteItem* item = m_palette->GetItemByHotkey(number);
	if (!item)
		return 0;
	if (size_t(item->userId) >= m_presetCollection->presets.size())
		return 0;
	return &m_presetCollection->presets[item->userId];
}

void AnimEventPresetPanel::Serialize(Serialization::IArchive& ar)
{
	ar(*m_palette, "palette");

	if (ar.isInput())
		WritePalette();

	if (ar.isInput())
		PresetsChanged();
}

static void UpdateEventContentToColorMap(EventContentToColorMap* contentMap, const AnimEventPresets& presets)
{
	contentMap->clear();

	vector<char> serializedEvent;
	for (size_t i = 0; i < presets.size(); ++i)
	{
		const AnimEventPreset& preset = presets[i];
		SerializeToMemory(&serializedEvent, Serialization::SStruct(preset.event));
		QColor color = PresetColor(preset.colorHue);
		contentMap->push_back(std::make_pair(CCrc32::Compute((void*)serializedEvent.data(), serializedEvent.size()), ColorB(color.red(), color.green(), color.blue(), 255)));
	}
	std::sort(contentMap->begin(), contentMap->end(),
	          [&](const std::pair<unsigned int, ColorB>& a, const std::pair<unsigned int, ColorB>& b) { return a.first < b.first; }
	          );
}

void AnimEventPresetPanel::PresetsChanged()
{
	UpdateEventContentToColorMap(&m_eventContentToColorMap, m_presetCollection->presets);
	SignalPresetsChanged();
}

static string PresetsFilename()
{
	string folder = GetIEditor()->GetUserFolder();
	folder += "\\CharacterTool\\AnimEventPresets.json";
	return folder;
}

void AnimEventPresetPanel::LoadPresets()
{
	Serialization::LoadJsonFile(*m_presetCollection, PresetsFilename().c_str());
}

void AnimEventPresetPanel::SavePresets()
{
	string filename = PresetsFilename();
	QDir().mkpath(QString::fromLocal8Bit(PathUtil::GetParentDirectory(filename.c_str())));
	Serialization::SaveJsonFile(filename.c_str(), *m_presetCollection);
}

}

