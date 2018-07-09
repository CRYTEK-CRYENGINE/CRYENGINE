// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "IUndoObject.h"
#include "Prefabs/PrefabManager.h"
#include "Objects/BaseObject.h"
#include "Objects/PrefabObject.h"

#include "PropertyRowPrefabLink.h"

#include "LogFile.h"

#include <CrySerialization/ClassFactory.h>
#include <IEditorImpl.h>
#include <IObjectManager.h>
#include <CrySerialization/yasli/Pointers.h>

struct PropertyRowPrefabLink::Picker : IPickObjectCallback
{
	// We need to be careful so that the picker's lifetime is not greater than the propertyrow's.
	// Normally, we would signal the picker when the row is destroyed, but since the row is destroyed during drawing
	// we need another way to track the lifetime of a row. One such way is to store a shared pointer to the row
	// and exit if we are the only owners.
	yasli::SharedPtr<PropertyRowPrefabLink> row;
	bool picking;
	PropertyTree*                           tree;
	CPrefabObject*                          prefab;

	Picker(PropertyRowPrefabLink* row, PropertyTree* tree)
		: row(row)
		, picking(false)
		, tree(tree)
	{
		prefab = row->GetOwner();
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

	bool OnPickFilter(CBaseObject* picked)
	{
		// check if row is still there. If not, we need to quit
		if (row->refCount() == 1)
		{
			GetIEditorImpl()->CancelPick();
			return false;
		}

		if (picked->CheckFlags(OBJFLAG_PREFAB) || picked == prefab)
			return false;

		return true;
	}

	void OnCancelPick() override
	{
		picking = false;
		if (row->refCount() > 1)
		{
			tree->repaint();
		}
	}
};

// ---------------------------------------------------------------------------

PrefabLinkMenuHandler::PrefabLinkMenuHandler(PropertyTree* tree, PropertyRowPrefabLink* self)
	: self(self), tree(tree)
{
}

void PrefabLinkMenuHandler::onMenuPick()
{
	self->pick(tree);
}

void PrefabLinkMenuHandler::onMenuSelect()
{
	self->select();
}

void PrefabLinkMenuHandler::onMenuDelete()
{
	PropertyRow* parentRow = self->parent();
	tree->model()->rowAboutToBeChanged(parentRow);
	self->clear();
	parentRow->erase(self);
	tree->model()->rowChanged(parentRow);
}

void PrefabLinkMenuHandler::onMenuExtract()
{
	CBaseObject* obj = GetIEditorImpl()->GetObjectManager()->FindObject(self->GetGUID());

	if (obj)
	{
		std::vector<CBaseObject*> objectsToExtract;
		objectsToExtract.push_back(obj);
		GetIEditorImpl()->GetPrefabManager()->ExtractObjectsFromPrefabs(objectsToExtract);
	}
}

void PrefabLinkMenuHandler::onMenuClone()
{
	CBaseObject* obj = GetIEditorImpl()->GetObjectManager()->FindObject(self->GetGUID());

	if (obj)
	{
		std::vector<CBaseObject*> objectsToClone;
		objectsToClone.push_back(obj);
		GetIEditorImpl()->GetPrefabManager()->CloneObjectsFromPrefabs(objectsToClone);
	}
}

// ---------------------------------------------------------------------------

PropertyRowPrefabLink::~PropertyRowPrefabLink()
{
	if (picker_.get() && picker_->picking)
		GetIEditorImpl()->CancelPick();
}

void PropertyRowPrefabLink::select()
{
	IObjectManager* pObjectManager = GetIEditorImpl()->GetObjectManager();
	pObjectManager->ClearSelection();
	CBaseObject* pObject = pObjectManager->FindObject(guid_);
	CRY_ASSERT(pObject);
	GetIEditorImpl()->OpenGroup(pObject->GetParent());
	pObjectManager->SelectObject(pObject);
}

void PropertyRowPrefabLink::pick(PropertyTree* tree)
{
	if (!picker_)
		picker_.reset(new Picker(this, tree));
	picker_->picking = true;
	GetIEditorImpl()->PickObject(picker_.get());
	tree->repaint();
}

bool PropertyRowPrefabLink::onActivate(const PropertyActivationEvent& e)
{
	if (e.reason == e.REASON_RELEASE)
		return false;

	if (e.reason == e.REASON_DOUBLECLICK)
		select();

	return true;
}
void PropertyRowPrefabLink::setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar)
{
	PrefabLink* value = (PrefabLink*)ser.pointer();
	name_ = value->name;
	guid_ = value->guid;
	owner_guid_ = value->owner_guid_;
	owner_ = static_cast<CPrefabObject*>(GetIEditorImpl()->GetObjectManager()->FindObject(owner_guid_));
	type_ = ser.type();
}

bool PropertyRowPrefabLink::assignTo(const Serialization::SStruct& ser) const
{
	PrefabLink* output = (PrefabLink*)ser.pointer();
	output->guid = guid_;
	output->owner_guid_ = owner_guid_;
	output->name = name_;
	return true;
}

void PropertyRowPrefabLink::serializeValue(Serialization::IArchive& ar)
{
	ar(guid_, "guid");
	ar(name_, "name_");
	ar(owner_guid_, "ownerguid");
}

string PropertyRowPrefabLink::valueAsString() const
{
	if (picker_ && picker_->picking)
		return "Pick an Entity...";
	else
		return name_;
}

void PropertyRowPrefabLink::clear()
{
	name_.clear();
	guid_ = CryGUID::Null();
}

bool PropertyRowPrefabLink::onContextMenu(IMenu& menu, PropertyTree* tree)
{
	yasli::SharedPtr<PropertyRow> selfPointer(this);

	PrefabLinkMenuHandler* handler = new PrefabLinkMenuHandler(tree, this);

	menu.addAction("Pick", 0, handler, &PrefabLinkMenuHandler::onMenuPick);
	menu.addAction("Select", 0, handler, &PrefabLinkMenuHandler::onMenuSelect);
	menu.addAction("Delete", 0, handler, &PrefabLinkMenuHandler::onMenuDelete);
	menu.addAction("Clone", 0, handler, &PrefabLinkMenuHandler::onMenuClone);
	menu.addAction("Extract", 0, handler, &PrefabLinkMenuHandler::onMenuExtract);

	tree->addMenuHandler(handler);
	return true;
}

REGISTER_PROPERTY_ROW(PrefabLink, PropertyRowPrefabLink);
DECLARE_SEGMENT(PropertyRowPrefabLink)


