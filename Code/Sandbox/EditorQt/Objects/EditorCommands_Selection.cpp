// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "IEditorImpl.h"
#include "Objects/ObjectManager.h"

#include <Commands/ICommandManager.h>
#include <IUndoObject.h>
#include <Viewport.h>

namespace Private_EditorCommands
{
std::vector<std::string> PyGetNamesOfSelectedObjects()
{
	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	std::vector<std::string> result;
	const int selectionCount = pSel->GetCount();
	result.reserve(selectionCount);
	for (int i = 0; i < selectionCount; i++)
	{
		result.push_back(pSel->GetObject(i)->GetName().GetString());
	}
	return result;
}

void PySelectObject(const char* objName)
{
	CUndo undo("Select Object");

	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject)
		GetIEditorImpl()->GetObjectManager()->AddObjectToSelection(pObject);
}

void PyUnselectObjects(const std::vector<std::string>& names)
{
	CUndo undo("Unselect Objects");

	std::vector<CBaseObject*> pBaseObjects;
	for (int i = 0; i < names.size(); i++)
	{
		if (!GetIEditorImpl()->GetObjectManager()->FindObject(names[i].c_str()))
		{
			throw std::logic_error(string("\"") + names[i].c_str() + "\" is an invalid entity.");
		}
		pBaseObjects.push_back(GetIEditorImpl()->GetObjectManager()->FindObject(names[i].c_str()));
	}

	for (int i = 0; i < pBaseObjects.size(); i++)
	{
		GetIEditorImpl()->GetObjectManager()->UnselectObject(pBaseObjects[i]);
	}
}

void PySelectObjects(const std::vector<std::string>& names)
{
	CUndo undo("Select Objects");
	CBaseObject* pObject;
	for (size_t i = 0; i < names.size(); ++i)
	{
		pObject = GetIEditorImpl()->GetObjectManager()->FindObject(names[i].c_str());
		if (!pObject)
		{
			throw std::logic_error(string("\"") + names[i].c_str() + "\" is an invalid entity.");
		}
		GetIEditorImpl()->GetObjectManager()->AddObjectToSelection(pObject);
	}
}

int PyGetNumSelectedObjects()
{
	if (const CSelectionGroup* pGroup = GetIEditorImpl()->GetObjectManager()->GetSelection())
	{
		return pGroup->GetCount();
	}

	return 0;
}

boost::python::tuple PyGetSelectionCenter()
{
	if (const CSelectionGroup* pGroup = GetIEditorImpl()->GetObjectManager()->GetSelection())
	{
		if (pGroup->GetCount() == 0)
		{
			throw std::runtime_error("Nothing selected");
		}

		const Vec3 center = pGroup->GetCenter();
		return boost::python::make_tuple(center.x, center.y, center.z);
	}

	throw std::runtime_error("Nothing selected");
}

boost::python::tuple PyGetSelectionAABB()
{
	if (const CSelectionGroup* pGroup = GetIEditorImpl()->GetObjectManager()->GetSelection())
	{
		if (pGroup->GetCount() == 0)
		{
			throw std::runtime_error("Nothing selected");
		}

		const AABB aabb = pGroup->GetBounds();
		return boost::python::make_tuple(aabb.min.x, aabb.min.y, aabb.min.z, aabb.max.x, aabb.max.y, aabb.max.z);
	}

	throw std::runtime_error("Nothing selected");
}

void ClearSelection()
{
	CUndo undo("Clear Selection");
	GetIEditorImpl()->GetObjectManager()->ClearSelection();
}

void InvertSelection()
{
	CUndo undo("Invert Selection");
	GetIEditorImpl()->GetObjectManager()->InvertSelection();
}

void ToggleLockSelection()
{
	GetIEditorImpl()->LockSelection(!GetIEditorImpl()->IsSelectionLocked());
}

void GoToSelection()
{
	CViewport* pViewport = GetIEditorImpl()->GetActiveView();
	if (pViewport)
	{
		pViewport->CenterOnSelection();
	}
}

void SelectAndGoTo(const char* objName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);

	if (pObject)
	{
		CUndo undo("Select Object");
		GetIEditorImpl()->GetObjectManager()->SelectObject(pObject);
		GoToSelection();
	}

}

void HideSelectedObjects()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	if (!pSelection->IsEmpty())
	{
		CUndo undo("Hide");
		int selectionCount = pSelection->GetCount();
		for (int i = 0; i < selectionCount; i++)
		{
			pSelection->GetObject(i)->SetHidden(true);
		}
	}
}

void LockSelectedObjects()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	if (!pSelection->IsEmpty())
	{
		CUndo undo("Lock Selected Objects");
		// Making selection locked will remove objects from selection
		for (int i = pSelection->GetCount() - 1; i >= 0; --i)
		{
			pSelection->GetObject(i)->SetFrozen(true);
		}
	}
}
}

DECLARE_PYTHON_MODULE(selection);

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyGetNamesOfSelectedObjects, selection, get_object_names,
                                          "Get the name from selected object/objects.",
                                          "selection.get_object_names()");
REGISTER_COMMAND_REMAPPING(general, get_names_of_selected_objects, selection, get_object_names)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PySelectObject, selection, select_object,
                                     "Selects a specified object.",
                                     "selection.select_object(str objectName)");
REGISTER_COMMAND_REMAPPING(general, select_object, selection, select_object)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyUnselectObjects, selection, unselect_objects,
                                          "Unselects a list of objects.",
                                          "selection.unselect_objects(list [str objectName,])");
REGISTER_COMMAND_REMAPPING(general, unselect_objects, selection, unselect_objects)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PySelectObjects, selection, select_objects,
                                          "Selects a list of objects.",
                                          "selection.select_objects(list [str objectName,])");
REGISTER_COMMAND_REMAPPING(general, select_objects, selection, select_objects)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyGetNumSelectedObjects, selection, get_count,
                                     "Returns the number of selected objects",
                                     "selection.get_count()");
REGISTER_COMMAND_REMAPPING(general, get_num_selected, selection, get_count)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyGetSelectionCenter, selection, get_center,
                                     "Returns the center point of the selection group",
                                     "selection.get_center()");
REGISTER_COMMAND_REMAPPING(general, selection_center, selection, get_center)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyGetSelectionAABB, selection, get_aabb,
                                     "Returns the aabb of the selection group",
                                     "selection.get_aabb()");
REGISTER_COMMAND_REMAPPING(general, get_selection_aabb, selection, get_aabb)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::ClearSelection, selection, clear,
                                   CCommandDescription("Clears selection"))
REGISTER_EDITOR_UI_COMMAND_DESC(selection, clear, "Clear Selection", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionSelect_None, selection, clear)
REGISTER_COMMAND_REMAPPING(general, clear_selection, selection, clear)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::InvertSelection, selection, invert,
                                   CCommandDescription("Inverts selection"))
REGISTER_EDITOR_UI_COMMAND_DESC(selection, invert, "Invert Selection", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionSelect_Invert, selection, invert)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::ToggleLockSelection, selection, lock,
                                   CCommandDescription("Toggles selection locking"))
REGISTER_EDITOR_UI_COMMAND_DESC(selection, lock, "Lock Selection", "Ctrl+Shift+Space", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionLock_Selection, selection, lock)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::SelectAndGoTo, selection, select_and_go_to,
                                   CCommandDescription("Select object of given name and go to it's position").Param("objectName", "Name of object that should be selected and focused"))
REGISTER_COMMAND_REMAPPING(general, select_and_go_to_object, selection, select_and_go_to)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::GoToSelection, selection, go_to,
                                   CCommandDescription("Focus camera on selection"))
REGISTER_EDITOR_UI_COMMAND_DESC(selection, go_to, "Go to Selection", CKeyboardShortcut("G; Num+."), "icons:General/Go_To_Selection.ico", false)
REGISTER_COMMAND_REMAPPING(level, go_to_selection, selection, go_to)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::HideSelectedObjects, selection, hide_objects,
                                   CCommandDescription("Hide selected objects"));
REGISTER_EDITOR_UI_COMMAND_DESC(selection, hide_objects, "Hide", "H", "icons:General/Visibility_False.ico", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionHide_Selection, selection, hide_objects)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::LockSelectedObjects, selection, lock_objects,
                                   CCommandDescription("Locks selected objects"));
REGISTER_EDITOR_UI_COMMAND_DESC(selection, lock_objects, "Lock Selected Objects", "F", "icons:general_lock_true.ico", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionFreeze_Selection, selection, lock_objects)
REGISTER_COMMAND_REMAPPING(selection, make_objects_uneditable, selection, lock_objects)
