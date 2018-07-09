// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "Util/BoostPythonHelpers.h"

#include "AI/AIManager.h"

#include "IEditorImpl.h"
#include "GameExporter.h"
#include "GameEngine.h"

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

REGISTER_PYTHON_ENUM_BEGIN(ENavigationUpdateType, ai, navigation_update_type)
REGISTER_PYTHON_ENUM_ITEM(ENavigationUpdateType::Continuous, continuous)
REGISTER_PYTHON_ENUM_ITEM(ENavigationUpdateType::AfterStabilization, afterChange)
REGISTER_PYTHON_ENUM_ITEM(ENavigationUpdateType::Disabled, disabled)
REGISTER_PYTHON_ENUM_END

#define REGISTER_AGENT_TYPE_COMMAND(typeID, functionPtr, moduleName, functionName) \
	namespace Private_EditorCommands { void functionPtr##typeID() { functionPtr(typeID); }} \
	REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::functionPtr##typeID, moduleName, functionName##typeID, \
		CCommandDescription("Debug Agent Type "#typeID))

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
REGISTER_EDITOR_COMMAND_ICON(ai, show_navigation_areas, "icons:common/ai_show_navigation_areas.ico");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyVisualizeNavigationAccessibility, ai, visualize_navigation_accessibility,
	CCommandDescription("Visualize Navigation Accessibility"));
REGISTER_EDITOR_COMMAND_ICON(ai, visualize_navigation_accessibility, "icons:common/ai_visualize_navigation_accessibility.ico");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PySetNavigationUpdateContinuous, ai, set_navigation_update_continuous,
	CCommandDescription("Sets navigation update type to continuous"));
REGISTER_EDITOR_COMMAND_TEXT(ai, set_navigation_update_continuous, "Continuous");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PySetNavigationUpdateAfterChange, ai, set_navigation_update_afterchange,
	CCommandDescription("Sets navigation update type to after change"));
REGISTER_EDITOR_COMMAND_TEXT(ai, set_navigation_update_afterchange, "After Change");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PySetNavigationUpdateDisabled, ai, set_navigation_update_disabled,
	CCommandDescription("Sets navigation update type to disabled"));
REGISTER_EDITOR_COMMAND_TEXT(ai, set_navigation_update_disabled, "Disabled");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PySetNavigationUpdate, ai, set_navigation_update_type,
	"Sets the navigation update type (0 = continuous, 1 = afterChange, 2 = disabled)",
	"ai.set_navigation_update_type(int updateType)");

#undef REGISTER_AGENT_TYPE_COMMAND
