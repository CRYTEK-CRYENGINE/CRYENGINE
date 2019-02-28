// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PropertyRowTagList.h"
#include "Serialization/PropertyTree/IMenu.h"
#include "Serialization/PropertyTree/PropertyRowString.h"
#include "Serialization/QPropertyTree/QPropertyTree.h"
#include <CrySerialization/Decorators/TagList.h>
#include <CrySerialization/ClassFactory.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/Decorators/TagListImpl.h>
#include <QMenu>

PropertyRowTagList::PropertyRowTagList()
	: source_(0)
{
}

PropertyRowTagList::~PropertyRowTagList()
{
	if (source_)
		source_->Release();
}

void PropertyRowTagList::generateMenu(property_tree::IMenu& item, PropertyTree* tree, bool addActions)
{
	if (userReadOnly() || isFixedSize())
		return;

	if (!source_)
		return;

	unsigned int numGroups = source_->GroupCount();
	for (unsigned int group = 0; group < numGroups; ++group)
	{
		unsigned int tagCount = source_->TagCount(group);
		if (tagCount == 0)
			continue;
		const char* groupName = source_->GroupName(group);
		string title = string("From ") + groupName;
		IMenu* menu = item.addMenu(title);
		for (unsigned int tagIndex = 0; tagIndex < tagCount; ++tagIndex)
		{
			string str;
			str = source_->TagValue(group, tagIndex);
			const char* desc = source_->TagDescription(group, tagIndex);
			if (desc && desc[0] != '\0')
			{
				str += "\t";
				str += desc;
			}
			TagListMenuHandler* handler = new TagListMenuHandler();
			handler->tree = tree;
			handler->row = this;
			handler->tag = source_->TagValue(group, tagIndex);
			tree->addMenuHandler(handler);
			menu->addAction(str.c_str(), "", 0, handler, &TagListMenuHandler::onMenuAddTag);
		}
	}

	TagListMenuHandler* handler = new TagListMenuHandler();
	handler->tree = tree;
	handler->row = this;
	handler->tag = string();
	tree->addMenuHandler(handler);
	item.addAction("Add", "", 0, handler, &TagListMenuHandler::onMenuAddTag);

	PropertyRowContainer::generateMenu(item, tree, false);
}

void PropertyRowTagList::addTag(const char* tag, PropertyTree* tree)
{
	yasli::SharedPtr<PropertyRowTagList> ref(this);

	PropertyRow* child = addElement(tree, false);
	if (child && strcmp(child->typeName(), "string") == 0)
	{
		PropertyRowString* stringRow = static_cast<PropertyRowString*>(child);
		tree->model()->rowAboutToBeChanged(stringRow);
		stringRow->setValue(tag, stringRow->searchHandle(), stringRow->typeId());
		tree->model()->rowChanged(stringRow);
	}
}

void TagListMenuHandler::onMenuAddTag()
{
	row->addTag(tag.c_str(), tree);
}

void PropertyRowTagList::setValueAndContext(const Serialization::IContainer& value, Serialization::IArchive& ar)
{
	if (source_)
		source_->Release();
	source_ = ar.context<ITagSource>();
	if (source_)
		source_->AddRef();

	PropertyRowContainer::setValueAndContext(value, ar);
}

REGISTER_PROPERTY_ROW(TagList, PropertyRowTagList)
DECLARE_SEGMENT(PropertyRowTagList)

