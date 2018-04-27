// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QObject>
#include "../Shared/DBATable.h"

namespace CharacterTool
{

class FilterAnimationList;

struct SEditorDBAEntry
{
	SDBAEntry           entry;
	std::vector<string> matchingAnimations;

	void                Serialize(Serialization::IArchive& ar);
};

class FilterAnimationList;
class EditorDBATable : public QObject
{
	Q_OBJECT
public:
	EditorDBATable() : m_filterAnimationList(0) {}
	void                   SetFilterAnimationList(const FilterAnimationList* filterAnimationList) { m_filterAnimationList = filterAnimationList; }
	void                   Serialize(Serialization::IArchive& ar);

	void                   FindTags(std::vector<std::pair<string, string>>* tags) const;
	int                    FindDBAForAnimation(const SAnimationFilterItem& item) const;
	const SEditorDBAEntry* GetEntryByIndex(int index) const;

	void                   Reset();
	bool                   Load();
	bool                   Save();

signals:
	void SignalChanged();
private:
	void UpdateMatchingAnimations();

	std::vector<SEditorDBAEntry> m_entries;
	const FilterAnimationList*   m_filterAnimationList;
};

}

