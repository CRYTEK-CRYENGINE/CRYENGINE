// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include "Serialization/PropertyTreeLegacy/PropertyRowContainer.h"

struct ITagSource;

class PropertyRowTagList : public PropertyRowContainer
{
public:
	PropertyRowTagList();
	~PropertyRowTagList();
	void setValueAndContext(const yasli::ContainerInterface& value, Serialization::IArchive& ar) override;
	void generateMenu(property_tree::IMenu& item, PropertyTreeLegacy* tree, bool addActions) override;
	void addTag(const char* tag, PropertyTreeLegacy* tree);

private:
	using PropertyRow::setValueAndContext;
	ITagSource* source_;
};

struct TagListMenuHandler : public PropertyRowMenuHandler
{
	string              tag;
	PropertyRowTagList* row;
	PropertyTreeLegacy*       tree;

	void onMenuAddTag();
};
