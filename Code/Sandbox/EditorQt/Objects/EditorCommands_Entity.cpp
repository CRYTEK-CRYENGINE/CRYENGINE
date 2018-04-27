// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

// EditorCommon
#include <ICommandManager.h>
#include <IUndoObject.h>

// Sandbox
#include "IEditorImpl.h"
#include "Objects/ObjectManager.h"
#include "Objects/ObjectLayerManager.h"

#include "Objects/BrushObject.h"
#include "Objects/EntityObject.h"
#include "Objects/EnvironmentProbeObject.h"
#include "Objects/GeomEntity.h"

#include "Material/Material.h"

#include "HyperGraph/FlowGraphHelpers.h"

#include "Util/CubemapUtils.h"

//////////////////////////////////////////////////////////////////////////
namespace Private_EditorCommands
{
string GetEntityGeometryFile(const char* objName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject == NULL)
		return "";

	string result = "";
	if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		result = static_cast<CBrushObject*>(pObject)->GetGeometryFile();
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CGeomEntity)))
	{
		result = static_cast<CGeomEntity*>(pObject)->GetGeometryFile();
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		result = static_cast<CEntityObject*>(pObject)->GetEntityPropertyString("object_Model");
	}

	result.MakeLower();
	result.Replace("/", "\\");
	return result;
}

void SetEntityGeometryFile(const char* objName, const char* filePath)
{
	CUndo undo("Set entity geometry file");
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject == NULL)
		return;

	if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		static_cast<CBrushObject*>(pObject)->SetGeometryFile(filePath);
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CGeomEntity)))
	{
		static_cast<CGeomEntity*>(pObject)->SetGeometryFile(filePath);
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		static_cast<CEntityObject*>(pObject)->SetEntityPropertyString("object_Model", filePath);
	}
}

void AddEntityLink(const char* pObjectName, const char* pTargetName, const char* pName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pObjectName);
	if (!pObject)
	{
		throw std::runtime_error("Could not find object");
	}

	CBaseObject* pTargetObject = GetIEditorImpl()->GetObjectManager()->FindObject(pTargetName);
	if (!pTargetObject)
	{
		throw std::runtime_error("Could not find target object");
	}

	if (!pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)) || !pTargetObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		throw std::runtime_error("Both object and target must be entities");
	}

	if (strcmp(pName, "") == 0)
	{
		throw std::runtime_error("Please specify a name");
	}

	CUndo undo("Add entity link");
	CEntityObject* pEntity = static_cast<CEntityObject*>(pObject);
	pEntity->AddEntityLink(pName, pTargetObject->GetId());
}

void ReloadEntityScripts()
{
	LOADING_TIME_PROFILE_AUTO_SESSION("reload_entity_scripts");

	CEntityScriptRegistry::Instance()->Reload();

	SEntityEvent event;
	event.event = ENTITY_EVENT_RELOAD_SCRIPT;
	gEnv->pEntitySystem->SendEventToAll(event);

	GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_RELOAD_ENTITY);

	// an entity node can be changed either by editing a script or in schematyc.
	// Any graph can use an entity node, so reload everything.
	//FIXME: Schematyc is reloading all entities on editor idle, which is slow and potentially crashes here. Uncomment when that behaviour is reviewed
	//GetIEditorImpl()->GetFlowGraphManager()->ReloadGraphs();
}

void ReloadArcheTypes()
{
	if (gEnv->pEntitySystem)
	{
		gEnv->pEntitySystem->RefreshEntityArchetypesInRegistry();
	}
}

void CompileScript()
{
	////////////////////////////////////////////////////////////////////////
	// Use the Lua compiler to compile a script
	////////////////////////////////////////////////////////////////////////

	std::vector<string> files;
	if (CFileUtil::SelectMultipleFiles(EFILE_TYPE_ANY, files, "Lua Files (*.lua)|*.lua||", "Scripts"))
	{
		//////////////////////////////////////////////////////////////////////////
		// Lock resources.
		// Speed ups loading a lot.
		ISystem* pSystem = GetIEditorImpl()->GetSystem();
		pSystem->GetI3DEngine()->LockCGFResources();
		//////////////////////////////////////////////////////////////////////////
		for (int i = 0; i < files.size(); i++)
		{
			if (!CFileUtil::CompileLuaFile(files[i]))
				return;

			// No errors
			// Reload this lua file.
			GetIEditorImpl()->GetSystem()->GetIScriptSystem()->ReloadScript(files[i], false);
		}
		//////////////////////////////////////////////////////////////////////////
		// Unlock resources.
		// Some unneeded resources that were locked before may get released here.
		pSystem->GetI3DEngine()->UnlockCGFResources();
		//////////////////////////////////////////////////////////////////////////
	}
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::GetEntityGeometryFile, entity, get_geometry_file,
                                     "Gets the geometry file name of a given entity.",
                                     "entity.get_geometry_file(str geometryName)");
REGISTER_COMMAND_REMAPPING(general, get_entity_geometry_file, entity, get_geometry_file)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::SetEntityGeometryFile, entity, set_geometry_file,
                                     "Sets the geometry file name of a given entity.",
                                     "entity.set_geometry_file(str geometryName, str cgfName)");
REGISTER_COMMAND_REMAPPING(general, set_entity_geometry_file, entity, set_geometry_file)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::AddEntityLink, entity, add_entity_link,
                                     "Adds an entity link to objectName to targetName with name",
                                     "object.add_entity_link(str objectName, str targetName, str name)");
REGISTER_COMMAND_REMAPPING(general, add_entity_link, entity, add_entity_link)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::ReloadEntityScripts, entity, reload_all_scripts,
                                   CCommandDescription("Reloads all entity scripts"))
REGISTER_EDITOR_UI_COMMAND_DESC(entity, reload_all_scripts, "Reload All Entity Scripts", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionReload_Entity_Scripts, entity, reload_all_scripts)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::ReloadArcheTypes, entity, reload_all_archetypes,
                                   CCommandDescription("Reloads all archetypes"))
REGISTER_EDITOR_UI_COMMAND_DESC(entity, reload_all_archetypes, "Reload Archetypes", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionReload_Archetypes, entity, reload_all_archetypes)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::CompileScript, entity, compile_scripts,
	CCommandDescription("Compiles selected scripts"))
	REGISTER_EDITOR_UI_COMMAND_DESC(entity, compile_scripts, "Compile Script", "", "", false)
	REGISTER_COMMAND_REMAPPING(ui_action, actionCompile_Script, entity, compile_scripts)
