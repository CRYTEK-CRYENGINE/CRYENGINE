// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"

#include "Objects/Group.h"
#include "Objects/ObjectManager.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/PrefabObject.h"
#include "Prefabs/PrefabManager.h"
#include "IEditorImpl.h"

#include <Commands/ICommandManager.h>
#include <IUndoObject.h>
#include <Preferences/SnappingPreferences.h>

namespace Private_EditorCommands
{
class CAttachToGroup : public IPickObjectCallback
{
public:
	//! Called when object picked.
	virtual void OnPick(CBaseObject* pPickedObject)
	{
		assert(pPickedObject->GetType() == OBJTYPE_PREFAB || pPickedObject->GetType() == OBJTYPE_GROUP);

		IObjectManager* pObjectManager = GetIEditorImpl()->GetObjectManager();

		if (pPickedObject->GetType() == OBJTYPE_GROUP)
		{
			CUndo undo("Attach to Group");
			CGroup* pPickedGroup = static_cast<CGroup*>(pPickedObject);
			const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
			pSelection->FilterParents();
			std::vector<CBaseObject*> filteredObjects;
			filteredObjects.reserve(pSelection->GetFilteredCount());

			// Save filtered list locally because we will be doing selections, which will invalidate the filtered group
			for (int i = 0; i < pSelection->GetFilteredCount(); i++)
				filteredObjects.push_back(pSelection->GetFilteredObject(i));

			for (CBaseObject* pSelectedObject : filteredObjects)
			{
				if (ChildIsValid(pPickedObject, pSelectedObject))
				{
					pPickedGroup->AddMember(pSelectedObject);

					// If this is not an open group we are adding to, we must also update the selection
					if (!pPickedGroup->IsOpen())
					{
						pObjectManager->UnselectObject(pSelectedObject);
					}
				}
			}

			if (!pPickedGroup->IsOpen() && !pPickedGroup->IsSelected())
			{
				pObjectManager->SelectObject(pPickedGroup);
			}
		}
		else if (pPickedObject->GetType() == OBJTYPE_PREFAB)
		{
			CUndo undo("Attach to Prefab");

			CPrefabObject* pPickedGroup = static_cast<CPrefabObject*>(pPickedObject);
			GetIEditor()->GetPrefabManager()->AddSelectionToPrefab(pPickedGroup);
		}

		delete this;
	}
	//! Called when pick mode cancelled.
	virtual void OnCancelPick()
	{
		delete this;
	}
	//! Return true if specified object is pickable.
	virtual bool OnPickFilter(CBaseObject* pFilterObject)
	{
		if (pFilterObject->GetType() == OBJTYPE_PREFAB || pFilterObject->GetType() == OBJTYPE_GROUP)
			return true;
		else
			return false;
	}
private:
	bool ChildIsValid(CBaseObject* pParent, CBaseObject* pChild, int nDir = 3)
	{
		if (pParent == pChild)
			return false;

		CBaseObject* pObj;
		if (nDir & 1)
		{
			if (pObj = pChild->GetParent())
			{
				if (!ChildIsValid(pParent, pObj, 1))
				{
					return false;
				}
			}
		}
		if (nDir & 2)
		{
			for (int i = 0; i < pChild->GetChildCount(); i++)
			{
				if (pObj = pChild->GetChild(i))
				{
					if (!ChildIsValid(pParent, pObj, 2))
					{
						return false;
					}
				}
			}
		}
		return true;
	}
};

void CreateGroupFromSelection()
{
	const CSelectionGroup* selection = GetIEditorImpl()->GetSelection();
	selection->FilterParents();

	std::vector<CBaseObject*> objects;
	for (auto i = 0; i < selection->GetFilteredCount(); i++)
	{
		objects.push_back(selection->GetFilteredObject(i));
	}

	if (!CGroup::CanCreateFrom(objects))
		return;

	CGroup::CreateFrom(objects, selection->GetCenter());
}

void UngroupSelected()
{
	// Ungroup all groups in selection.
	std::vector<CBaseObjectPtr> objects;

	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	for (int i = 0; i < pSelection->GetCount(); i++)
	{
		objects.push_back(pSelection->GetObject(i));
	}

	if (objects.size())
	{
		CUndo undo("Ungroup");

		for (int i = 0; i < objects.size(); i++)
		{
			CBaseObject* obj = objects[i];
			if (obj && obj->GetRuntimeClass() == RUNTIME_CLASS(CGroup))
			{
				static_cast<CGroup*>(obj)->Ungroup();
				// Signal prefab if part of any
				if (CPrefabObject* pPrefab = (CPrefabObject*)obj->GetPrefab())
				{
					pPrefab->RemoveMember(obj);
				}
				GetIEditorImpl()->DeleteObject(obj);
			}
		}
	}
}

void OpenGroup()
{
	// Ungroup all groups in selection.
	const CSelectionGroup* sel = GetIEditorImpl()->GetSelection();
	if (!sel->IsEmpty())
	{
		CUndo undo("Group Open");
		for (int i = 0; i < sel->GetCount(); i++)
		{
			CBaseObject* pObject = sel->GetObject(i);
			if (pObject && pObject->GetRuntimeClass() == RUNTIME_CLASS(CGroup))
			{
				static_cast<CGroup*>(pObject)->Open();
			}
		}
	}
	GetIEditorImpl()->SetModifiedFlag();
}

void CloseGroup()
{
	// Ungroup all groups in selection.
	const CSelectionGroup* sel = GetIEditorImpl()->GetSelection();
	if (!sel->IsEmpty())
	{
		CUndo undo("Group Open");
		for (int i = 0; i < sel->GetCount(); i++)
		{
			CBaseObject* pObject = sel->GetObject(i);
			if (pObject && pObject->GetRuntimeClass() == RUNTIME_CLASS(CGroup))
			{
				static_cast<CGroup*>(pObject)->Close();
			}
		}
	}
	GetIEditorImpl()->SetModifiedFlag();
}

void AttachToGroup()
{
	CAttachToGroup* pCallback = new CAttachToGroup;
	GetIEditorImpl()->GetLevelEditorSharedState()->PickObject(pCallback);
}

//! Add objects identified by name to a group identified by name  
void AddObjectsToGroup(const std::vector<std::string>& objectsToAttachNames, const std::string& groupName)
{
	//Register the Attach undo
	CUndo undo("Add to Group");
	
	//Get the group object from the groupName
	CBaseObject* pGroup = GetIEditorImpl()->GetObjectManager()->FindObject(groupName.c_str());
	
	//Make sure the name refers to an object and that object is a group
	if (!pGroup)
	{
		throw std::logic_error(string("\"") + groupName.c_str() + "\" is not an existing group.");
	}
	if (!pGroup->IsKindOf(RUNTIME_CLASS(CGroup)))
	{
		throw std::logic_error(string("\"") + groupName.c_str() + "\" is not a group.");
	}
	//Now go through all the object names, get the objects and add them to the group
	for (size_t i = 0; i < objectsToAttachNames.size(); ++i)
	{
		CBaseObject* pObjectToAttach = GetIEditorImpl()->GetObjectManager()->FindObject(objectsToAttachNames[i].c_str());
		if (!pObjectToAttach)
		{
			throw std::logic_error(string("\"") + objectsToAttachNames[i].c_str() + "\" is an invalid object.");
		}
		pGroup->AddMember(pObjectToAttach);
	}
}

//!Detach all the objects in a selection group from the group they are in
void Detach(const CSelectionGroup& selection, bool shouldKeepPosition, bool shouldPlaceOnRoot)
{
	for (int i = 0, count = selection.GetCount(); i < count; ++i)
	{
		if (selection.GetObject(i)->GetGroup())
		{
			CGroup* pGroup = static_cast<CGroup*>(selection.GetObject(i)->GetGroup());
			pGroup->RemoveMember(selection.GetObject(i), shouldKeepPosition, shouldPlaceOnRoot);
		}
	}
}
//! Detach all the objects in the current editor selection from the group they are in 
void DetachSelectionFromGroup()
{
	CUndo undo("Detach from Group");
	Detach(*GetIEditorImpl()->GetSelection(), true, false);
}

//! Detach all the objects in the current editor selection from the group they are in to the root of their hierarchy
void DetachToRoot()
{
	CUndo undo("Detach Selection from Hierarchy");
	Detach(*GetIEditorImpl()->GetSelection(), true, true);
}

//! Detach all the objects identified by name from the group they are in 
//! \param toRoot detach the objects to the root of their hierarchy
void DetachObjectsFromNames(const std::vector<std::string>& objectsToDetachNames, bool toRoot)
{
	//Create a selection group with the objects to detach and pass it to the detach function
	CSelectionGroup selectionGroup;
	for (auto const& name : objectsToDetachNames)
	{
		CBaseObject* pObjectToDetach = GetIEditorImpl()->GetObjectManager()->FindObject(name.c_str());
		if (!pObjectToDetach)
		{
			throw std::logic_error(string("\"") + name.c_str() + "\" is an invalid object.");
		}
		selectionGroup.AddObject(pObjectToDetach);
	}

	Detach(selectionGroup, true, toRoot);
}

//! Detach all the objects identified by name from the group they are in to the root of their hierarchy and register an undo
void DetachObjectsFromGroup(const std::vector<std::string>& objectsToDetachNames)
{
	CUndo undo("Detach from Group");
	DetachObjectsFromNames(objectsToDetachNames, false);
}

//! Detach all the objects identified by name from the group they are in and register an undo
void DetachObjectsToRoot(const std::vector<std::string>& objectsToDetachNames)
{
	CUndo undo("Detach from Hierarchy");
	DetachObjectsFromNames(objectsToDetachNames, true);
}
}

DECLARE_PYTHON_MODULE(group);

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::CreateGroupFromSelection, group, create_from_selection,
                                   CCommandDescription("Creates new group from selected objects"))
REGISTER_EDITOR_UI_COMMAND_DESC(group, create_from_selection, "Create Group", "Ctrl+Shift+G", "icons:General/Group.ico", false);
REGISTER_COMMAND_REMAPPING(ui_action, actionGroup, group, create_from_selection)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::UngroupSelected, group, ungroup,
                                   CCommandDescription("Ungroups objects in selected groups"))
REGISTER_EDITOR_UI_COMMAND_DESC(group, ungroup, "Ungroup", "", "icons:General/UnGroup.ico", false);
REGISTER_COMMAND_REMAPPING(ui_action, actionUngroup, group, ungroup)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::OpenGroup, group, open,
                                   CCommandDescription("Opens selected groups"))
REGISTER_EDITOR_UI_COMMAND_DESC(group, open, "Open", "", "", false);
REGISTER_COMMAND_REMAPPING(ui_action, actionGroup_Open, group, open)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::CloseGroup, group, close,
                                   CCommandDescription("Closes selected groups"))
REGISTER_EDITOR_UI_COMMAND_DESC(group, close, "Close", "", "", false);
REGISTER_COMMAND_REMAPPING(ui_action, actionGroup_Close, group, close)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::AttachToGroup, group, attach_to,
                                   CCommandDescription("Attaches the selected objects to the picked group"))
REGISTER_EDITOR_UI_COMMAND_DESC(group, attach_to, "Attach to...", "", "", false);
REGISTER_COMMAND_REMAPPING(ui_action, actionGroup_Attach, group, attach_to)

REGISTER_ONLY_PYTHON_COMMAND(Private_EditorCommands::AddObjectsToGroup, group, attach_objects_to,
                             CCommandDescription("Attaches a list of objects referenced by their names to a group also referenced by name").Param("objectsToAttachNames", "A list of names containing the objects to attach").Param("groupName", "The name of the group we want to attach the objects to"))

REGISTER_ONLY_PYTHON_COMMAND(Private_EditorCommands::DetachObjectsFromGroup, group, detach_objects_from,
                             CCommandDescription("Detaches a list of objects referenced by their names from the group they are in").Param("objectsToDetachNames", "A list of names containing the objects to detach"))

REGISTER_ONLY_PYTHON_COMMAND(Private_EditorCommands::DetachObjectsToRoot, group, detach_objects_to_root,
                             CCommandDescription("Detaches a list of objects referenced by their names to the root of the hierarchy").Param("objectsToDetachNames", "A list of names containing the objects to detach"))

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::DetachSelectionFromGroup, group, detach,
                                   CCommandDescription("Detaches selected objects from group. The object will be placed relative to the group's parent (or layer if the group doesn't have a parent)"))
REGISTER_EDITOR_UI_COMMAND_DESC(group, detach, "Detach", "", "", false);
REGISTER_COMMAND_REMAPPING(ui_action, actionGroup_Detach, group, detach)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::DetachToRoot, group, detach_from_hierarchy,
                                   CCommandDescription("Detaches selected objects from full hierarchy. The object will placed directly on it's owning layer"))
REGISTER_EDITOR_UI_COMMAND_DESC(group, detach_from_hierarchy, "Detach from Hierarchy", "", "", false);
REGISTER_COMMAND_REMAPPING(ui_action, actionGroup_DetachToRoot, group, detach_from_hierarchy)