// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <memory>
#include <QWidget>
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Color.h>
#include <CrySerialization/Forward.h>

class BlockPalette;
struct BlockPaletteItem;
class QPropertyTree;

namespace CharacterTool {

struct System;
struct AnimEventPreset;
struct AnimEventPresetCollection;

typedef std::vector<std::pair<unsigned int, ColorB>> EventContentToColorMap;

class AnimEventPresetPanel : public QWidget
{
	Q_OBJECT
public:
	AnimEventPresetPanel(QWidget* parent, System* system);
	~AnimEventPresetPanel();

	const AnimEventPreset*        GetPresetByHotkey(int number) const;
	const EventContentToColorMap& GetEventContentToColorMap() const { return m_eventContentToColorMap; }
	void                          LoadPresets();
	void                          SavePresets();

	void                          Serialize(Serialization::IArchive& ar);
signals:
	void                          SignalPutEvent(const AnimEventPreset& preset);
	void                          SignalPresetsChanged();
protected slots:
	void                          OnPropertyTreeChanged();
	void                          OnPaletteSelectionChanged();
	void                          OnPaletteItemClicked(const BlockPaletteItem& item);
	void                          OnPaletteChanged();

private:
	void WritePalette();
	void ReadPalette();
	void PresetsChanged();
	EventContentToColorMap                     m_eventContentToColorMap;
	BlockPalette*                              m_palette;
	QPropertyTree*                             m_tree;
	std::unique_ptr<AnimEventPresetCollection> m_presetCollection;
};

}

