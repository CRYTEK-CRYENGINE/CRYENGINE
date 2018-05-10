// Copyright 2001-2016 Crytek GmbH. All rights reserved.
#include <StdAfx.h>

#include "Util/BoostPythonHelpers.h"

#include "AI/AIManager.h"

#include "IEditorImpl.h"
#include "GameExporter.h"
#include "GameEngine.h"
#include "Objects/EntityObject.h"

#include <CryAISystem/IAIObjectManager.h>

namespace Private_EditorCommands
{
void PySetNavigationUpdate(int updateType)
{
	GetIEditorImpl()->GetAI()->SetNavigationUpdateType((ENavigationUpdateType)updateType);
}

void PySetNavigationUpdateContinuous()
{
	GetIEditorImpl()->GetAI()->SetNavigationUpdateType(ENavigationUpdateType::Continuous);
}

void PySetNavigationUpdateAfterChange()
{
	GetIEditorImpl()->GetAI()->SetNavigationUpdateType(ENavigationUpdateType::AfterStabilization);
}

void PySetNavigationUpdateDisabled()
{
	GetIEditorImpl()->GetAI()->SetNavigationUpdateType(ENavigationUpdateType::Disabled);
}

void PyRegenerateMNMType(const char* type)
{
	CAIManager* manager = GetIEditorImpl()->GetAI();
	manager->RegenerateNavigationByTypeName(type);
}

void PyRegenerateMNMAll()
{
	CAIManager* manager = GetIEditorImpl()->GetAI();
	manager->RegenerateNavigationByTypeName("all");
}

void PyGenerateCoverSurfaces()
{
	GetIEditorImpl()->GetGameEngine()->GenerateAICoverSurfaces();

	// Do game export
	CGameExporter gameExporter;
	gameExporter.Export(eExp_AI_CoverSurfaces);
}

void PyNavigationShowAreas()
{
	gAINavigationPreferences.setNavigationShowAreas(!gAINavigationPreferences.navigationShowAreas());
}

void PyVisualizeNavigationAccessibility()
{
	gAINavigationPreferences.setVisualizeNavigationAccessibility(!gAINavigationPreferences.visualizeNavigationAccessibility());
}

void ReloadAIScripts()
{
	GetIEditorImpl()->GetAI()->ReloadScripts();

	// grab the entity IDs of all AI objects
	std::vector<EntityId> entityIDsUsedByAIObjects;
	IAIObjectIter* pIter = GetIEditorImpl()->GetAI()->GetAISystem()->GetAIObjectManager()->GetFirstAIObject(OBJFILTER_TYPE, 0);
	for (IAIObject* pAI = pIter->GetObject(); pAI != NULL; pIter->Next(), pAI = pIter->GetObject())
	{
		entityIDsUsedByAIObjects.push_back(pAI->GetEntityID());
	}
	pIter->Release();

	// find the AI objects among all editor objects
	CBaseObjectsArray allEditorObjects;
	GetIEditorImpl()->GetObjectManager()->GetObjects(allEditorObjects);
	for (CBaseObjectsArray::const_iterator it = allEditorObjects.begin(); it != allEditorObjects.end(); ++it)
	{
		CBaseObject* obj = *it;

		if (obj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* entity = static_cast<CEntityObject*>(obj);
			const EntityId entityId = entity->GetEntityId();

			// is this entity using AI? -> reload the entity and its script
			if (std::find(entityIDsUsedByAIObjects.begin(), entityIDsUsedByAIObjects.end(), entityId) != entityIDsUsedByAIObjects.end())
			{
				if (CEntityScript* script = entity->GetScript())
				{
					script->Reload();
				}
				entity->Reload(true);

				if (IEntity* pEnt = gEnv->pEntitySystem->GetEntity(entityId))
				{
					SEntityEvent event;
					event.event = ENTITY_EVENT_RELOAD_SCRIPT;
					pEnt->SendEvent(event);
				}
			}
		}
	}
}

void PySetDebugAgentType(const int typeIdx)
{
	const bool shouldBeDisplayed = gAINavigationPreferences.navigationDebugAgentType() != typeIdx || !gAINavigationPreferences.navigationDebugDisplay();
	gAINavigationPreferences.setNavigationDebugAgentType(typeIdx);
	gAINavigationPreferences.setNavigationDebugDisplay(shouldBeDisplayed);
}

void PyRegenerateAgentTypeLayer(const int typeIdx)
{
	CAIManager* manager = GetIEditorImpl()->GetAI();
	if (INavigationSystem* pNavigationSystem = manager->GetAISystem()->GetNavigationSystem())
	{
		const NavigationAgentTypeID navigationAgentTypeID = pNavigationSystem->GetAgentTypeID(typeIdx);
		if (navigationAgentTypeID != NavigationAgentTypeID())
		{
			pNavigationSystem->GetUpdateManager()->RequestGlobalUpdateForAgentType(navigationAgentTypeID);
		}
	}
}
}

REGISTER_PYTHON_ENUM_BEGIN(ENavigationUpdateType, ai, navigation_update_type)
REGISTER_PYTHON_ENUM_ITEM(ENavigationUpdateType::Continuous, continuous)
REGISTER_PYTHON_ENUM_ITEM(ENavigationUpdateType::AfterStabilization, afterChange)
REGISTER_PYTHON_ENUM_ITEM(ENavigationUpdateType::Disabled, disabled)
REGISTER_PYTHON_ENUM_END

#define REGISTER_AGENT_TYPE_COMMAND(typeID, functionPtr, moduleName, functionName)                                      \
  namespace Private_EditorCommands { void functionPtr ## typeID() { functionPtr(typeID); }                              \
  }                                                                                                                     \
  REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::functionPtr ## typeID, moduleName, functionName ## typeID, \
                                     CCommandDescription("Debug Agent Type " # typeID))

REGISTER_AGENT_TYPE_COMMAND(0, PySetDebugAgentType, ai, debug_agent_type);
REGISTER_AGENT_TYPE_COMMAND(1, PySetDebugAgentType, ai, debug_agent_type);
REGISTER_AGENT_TYPE_COMMAND(2, PySetDebugAgentType, ai, debug_agent_type);
REGISTER_AGENT_TYPE_COMMAND(3, PySetDebugAgentType, ai, debug_agent_type);
REGISTER_AGENT_TYPE_COMMAND(4, PySetDebugAgentType, ai, debug_agent_type);
REGISTER_AGENT_TYPE_COMMAND(5, PySetDebugAgentType, ai, debug_agent_type);

REGISTER_AGENT_TYPE_COMMAND(0, PyRegenerateAgentTypeLayer, ai, regenerate_agent_type_layer)
REGISTER_AGENT_TYPE_COMMAND(1, PyRegenerateAgentTypeLayer, ai, regenerate_agent_type_layer)
REGISTER_AGENT_TYPE_COMMAND(2, PyRegenerateAgentTypeLayer, ai, regenerate_agent_type_layer)
REGISTER_AGENT_TYPE_COMMAND(3, PyRegenerateAgentTypeLayer, ai, regenerate_agent_type_layer)
REGISTER_AGENT_TYPE_COMMAND(4, PyRegenerateAgentTypeLayer, ai, regenerate_agent_type_layer)
REGISTER_AGENT_TYPE_COMMAND(5, PyRegenerateAgentTypeLayer, ai, regenerate_agent_type_layer)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyGenerateCoverSurfaces, ai, generate_cover_surfaces,
                                   CCommandDescription("Generate Cover Surfaces"));

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyRegenerateMNMType, ai, regenerate_mnm_type,
                                     "Regenerate navmesh for specified agent type ('all' for all types)",
                                     "ai.regenerate_mnm_type(string agentType)");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyRegenerateMNMAll, ai, regenerate_agent_type_all,
                                   CCommandDescription("Regenerate All Layers"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyNavigationShowAreas, ai, show_navigation_areas,
                                   CCommandDescription("Show Navigation Areas"));
REGISTER_EDITOR_UI_COMMAND_DESC(ai, show_navigation_areas, "Show Navigation Areas", "",
                                "icons:common/ai_show_navigation_areas.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyVisualizeNavigationAccessibility, ai, visualize_navigation_accessibility,
                                   CCommandDescription("Visualize Navigation Accessibility"));
REGISTER_EDITOR_UI_COMMAND_DESC(ai, visualize_navigation_accessibility, "Visualize Navigation Accessibility", "",
                                "icons:common/ai_visualize_navigation_accessibility.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PySetNavigationUpdateContinuous, ai, set_navigation_update_continuous,
                                   CCommandDescription("Sets navigation update type to continuous"));
REGISTER_EDITOR_UI_COMMAND_DESC(ai, set_navigation_update_continuous, "Continuous", "", "", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PySetNavigationUpdateAfterChange, ai, set_navigation_update_afterchange,
                                   CCommandDescription("Sets navigation update type to after change"));
REGISTER_EDITOR_UI_COMMAND_DESC(ai, set_navigation_update_afterchange, "After Change", "", "", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PySetNavigationUpdateDisabled, ai, set_navigation_update_disabled,
                                   CCommandDescription("Sets navigation update type to disabled"));
REGISTER_EDITOR_UI_COMMAND_DESC(ai, set_navigation_update_disabled, "Disabled", "", "", true)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PySetNavigationUpdate, ai, set_navigation_update_type,
                                     "Sets the navigation update type (0 = continuous, 1 = afterChange, 2 = disabled)",
                                     "ai.set_navigation_update_type(int updateType)");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::ReloadAIScripts, ai, reload_all_scripts,
                                   CCommandDescription("Reloads all AI scripts"))
REGISTER_EDITOR_UI_COMMAND_DESC(ai, reload_all_scripts, "Reload All AI Scripts", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionReload_AI_Scripts, ai, reload_all_scripts)

#undef REGISTER_AGENT_TYPE_COMMAND
