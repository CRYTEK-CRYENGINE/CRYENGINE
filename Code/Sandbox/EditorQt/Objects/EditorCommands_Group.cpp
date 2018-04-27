// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

// EditorCommon
#include <ICommandManager.h>
#include <IUndoObject.h>
#include <Grid.h>

// Sandbox
#include "IEditorImpl.h"
#include "Objects/ObjectManager.h"
#include "Objects/ObjectLayerManager.h"

#include "Objects/Group.h"
#include "Objects/PrefabObject.h"
#include "Prefabs/PrefabManager.h"

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
	GetIEditorImpl()->PickObject(pCallback);
}

void Detach_Impl(bool shouldKeepPosition, bool shouldPlaceOnRoot)
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	for (int i = 0, cnt = pSelection->GetCount(); i < cnt; ++i)
	{
		if (pSelection->GetObject(i)->GetGroup())
		{
			CGroup* pGroup = static_cast<CGroup*>(pSelection->GetObject(i)->GetGroup());
			pGroup->RemoveMember(pSelection->GetObject(i), shouldKeepPosition, shouldPlaceOnRoot);
		}
	}
}

void DetachFromGroup()
{
	CUndo undo("Detach from Group");
	Detach_Impl(true, false);
}

void DetachToRoot()
{
	CUndo undo("Detach Selection from Hierarchy");
	Detach_Impl(true, true);
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

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::DetachFromGroup, group, detach,
                                   CCommandDescription("Detaches selected objects from group. The object will be placed relative to the group's parent (or layer if the group doesn't have a parent)"))
REGISTER_EDITOR_UI_COMMAND_DESC(group, detach, "Detach", "", "", false);
REGISTER_COMMAND_REMAPPING(ui_action, actionGroup_Detach, group, detach)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::DetachFromGroup, group, detach_from_hierarchy,
                                   CCommandDescription("Detaches selected objects from full hierarchy. The object will placed directly on it's owning layer"))
REGISTER_EDITOR_UI_COMMAND_DESC(group, detach_from_hierarchy, "Detach from Hierarchy", "", "", false);
REGISTER_COMMAND_REMAPPING(ui_action, actionGroup_DetachToRoot, group, detach_from_hierarchy)
