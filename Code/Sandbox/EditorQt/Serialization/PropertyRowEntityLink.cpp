// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <stdafx.h>
#include "PropertyRowEntityLink.h"

#include <CrySerialization/ClassFactory.h>
#include <IEditor.h>
#include <IObjectManager.h>
#include <CrySerialization/yasli/Pointers.h>

#include "Objects/EntityObject.h"

struct PropertyRowEntityLink::Picker : IPickObjectCallback
{
	PropertyRowEntityLink* row;
	bool picking;
	PropertyTree*                           tree;

	Picker(PropertyRowEntityLink* row, PropertyTree* tree)
		: row(row)
		, picking(false)
		, tree(tree)
	{
	}

	void OnPick(CBaseObject* obj) override
	{
		picking = false;
		if (row->refCount() > 1)
		{
			tree->model()->rowAboutToBeChanged(row);
			row->name_ = obj->GetName();
			row->guid_ = obj->GetId();
			tree->model()->rowChanged(row);
		}
	}

	void OnCancelPick() override
	{
		picking = false;
		if (tree)
		{
			tree->repaint();
		}
	}
};

// ---------------------------------------------------------------------------

EntityLinkMenuHandler::EntityLinkMenuHandler(PropertyTree* tree, PropertyRowEntityLink* self)
	: self(self), tree(tree)
{
}

void EntityLinkMenuHandler::onMenuPick()
{
	if (!self->isPicking())
		self->pick(tree);
}

void EntityLinkMenuHandler::onMenuSelect()
{
	self->select();
}

void EntityLinkMenuHandler::onMenuDelete()
{
	PropertyRow* parentRow = self->parent();
	tree->model()->rowAboutToBeChanged(parentRow);
	self->clear();
	parentRow->erase(self);
	tree->model()->rowChanged(parentRow);
}

// ---------------------------------------------------------------------------

PropertyRowEntityLink::~PropertyRowEntityLink()
{
	if (isPicking())
	{
		// Make sure the picker's tree is set to null since it's in the process of getting destroyed.
		// Calling repaint on the tree will lead to a crash
		picker_->tree = nullptr;
		GetIEditorImpl()->CancelPick();
	}
}

void PropertyRowEntityLink::select()
{
	IObjectManager* manager = GetIEditorImpl()->GetObjectManager();
	manager->ClearSelection();

	if (CBaseObject* obj = manager->FindObject(guid_))
		manager->SelectObject(obj);
}

void PropertyRowEntityLink::pick(PropertyTree* tree)
{
	if (!picker_)
		picker_.reset(new Picker(this, tree));
	picker_->picking = true;
	GetIEditorImpl()->PickObject(picker_.get());
	tree->repaint();
}

bool PropertyRowEntityLink::onActivate(const PropertyActivationEvent& e)
{
	if (e.reason == e.REASON_RELEASE)
		return false;

	if (!isPicking())
		pick(e.tree);
	return true;
}

void PropertyRowEntityLink::serializeValue(Serialization::IArchive& ar)
{
	ar(guid_, "guid");
	ar(name_, "name_");
}

property_tree::Icon PropertyRowEntityLink::buttonIcon(const PropertyTree* tree, int index) const
{
	return property_tree::Icon("icons:Navigation/Basics_Select.ico");
}

bool PropertyRowEntityLink::isPicking() const
{
	return picker_ && picker_->picking;
}

string PropertyRowEntityLink::valueAsString() const
{
	if (isPicking())
		return "Pick an Entity...";
	else
		return name_;
}

void PropertyRowEntityLink::clear()
{
	name_.clear();
	guid_ = CryGUID::Null();
}

bool PropertyRowEntityLink::onContextMenu(IMenu& menu, PropertyTree* tree)
{
	yasli::SharedPtr<PropertyRow> selfPointer(this);

	EntityLinkMenuHandler* handler = new EntityLinkMenuHandler(tree, this);

	menu.addAction("Pick", 0, handler, &EntityLinkMenuHandler::onMenuPick);
	menu.addAction("Select", 0, handler, &EntityLinkMenuHandler::onMenuSelect);
	menu.addAction("Delete", 0, handler, &EntityLinkMenuHandler::onMenuDelete);

	tree->addMenuHandler(handler);
	return true;
}

class PropertyRowEntityTargetImpl final : public PropertyRowEntityLink
{
	void setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar)
	{
		Serialization::EntityTarget* value = (Serialization::EntityTarget*)ser.pointer();
		name_ = value->name;
		guid_ = value->guid;
		type_ = ser.type();
	}

	bool assignTo(const Serialization::SStruct& ser) const
	{
		Serialization::EntityTarget* output = (Serialization::EntityTarget*)ser.pointer();
		output->guid = guid_;
		output->name = name_;
		return true;
	}
};

REGISTER_PROPERTY_ROW(Serialization::EntityTarget, PropertyRowEntityTargetImpl);
DECLARE_SEGMENT(PropertyRowEntityLink)



