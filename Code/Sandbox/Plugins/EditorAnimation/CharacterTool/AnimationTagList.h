// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Decorators/TagList.h>
#include <QObject>

namespace CharacterTool
{

class EditorCompressionPresetTable;
class EditorDBATable;
class AnimationTagList : public QObject, public ITagSource
{
	Q_OBJECT
public:
	AnimationTagList(EditorCompressionPresetTable* presets, EditorDBATable* dbaTable);

protected:
	void         AddRef() override  {}
	void         Release() override {}

	unsigned int TagCount(unsigned int group) const override;
	const char*  TagValue(unsigned int group, unsigned int index) const override;
	const char*  TagDescription(unsigned int group, unsigned int index) const override;
	const char*  GroupName(unsigned int group) const override;
	unsigned int GroupCount() const override;
protected slots:
	void         OnCompressionPresetsChanged();
	void         OnDBATableChanged();
private:
	EditorCompressionPresetTable* m_presets;
	EditorDBATable*               m_dbaTable;

	struct SEntry
	{
		string tag;
		string description;
	};

	struct SGroup
	{
		string              name;
		std::vector<SEntry> entries;
	};

	std::vector<SGroup> m_groups;
};

}

