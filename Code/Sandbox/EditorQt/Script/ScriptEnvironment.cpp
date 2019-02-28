// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScriptEnvironment.h"

#include "IEditorImpl.h"
#include "Objects/EntityObject.h"

EditorScriptEnvironment::EditorScriptEnvironment()
{
	GetIEditorImpl()->RegisterNotifyListener(this);
}

EditorScriptEnvironment::~EditorScriptEnvironment()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);
}

void EditorScriptEnvironment::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnInit)
	{
		RegisterWithScriptSystem();
	}

	if (event == eNotify_OnSelectionChange)
	{
		SelectionChanged();
	}
}

void EditorScriptEnvironment::RegisterWithScriptSystem()
{
	Init(gEnv->pScriptSystem, gEnv->pSystem);
	SetGlobalName("Editor");

	m_selection = SmartScriptTable(gEnv->pScriptSystem, false);
	m_pMethodsTable->SetValue("Selection", m_selection);

	// Editor.* functions go here
	RegisterTemplateFunction("Command", "commandName", *this, &EditorScriptEnvironment::Command);
}

void EditorScriptEnvironment::SelectionChanged()
{
	if (!m_selection)
		return;

	m_selection->Clear();

	const CSelectionGroup* selectionGroup = GetIEditorImpl()->GetSelection();
	int selectionCount = selectionGroup->GetCount();

	for (int i = 0, k = 0; i < selectionCount; ++i)
	{
		CBaseObject* object = selectionGroup->GetObject(i);
		if (object->GetType() == OBJTYPE_ENTITY)
		{
			CEntityObject* entity = (CEntityObject*)object;

			if (IEntity* iEntity = entity->GetIEntity())
				m_selection->SetAt(++k, ScriptHandle(iEntity->GetId()));
		}
	}
}

int EditorScriptEnvironment::Command(IFunctionHandler* pH, const char* commandName)
{
	GetIEditorImpl()->GetCommandManager()->Execute(commandName);

	return pH->EndFunction();
}

