// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 27:07:2010   Extracted from BodyDamage.h/.cpp by Benito Gangoso Rodriguez

*************************************************************************/

#pragma once

#ifndef BODY_DAMAGE_MANAGER_CVARS_H
#define BODY_DAMAGE_MANAGER_CVARS_H

class CBodyManagerCVars
{
	friend class CBodyDamageProfile;
public:
	static void RegisterCommands();
	static void RegisterVariables();
	static void UnregisterCommands(IConsole* pConsole);
	static void UnregisterVariables(IConsole* pConsole);

	static int IsBodyDamageLogEnabled() { return g_bodyDamage_log; }
	static int IsBodyDestructionDebugEnabled() { return g_bodyDestruction_debug; }
	static bool IsBodyDestructionDebugFilterFor(const char* entityName)
	{
		CRY_ASSERT(entityName);
		CRY_ASSERT(g_bodyDestruction_debugFilter);

		const char* filter = g_bodyDestruction_debugFilter->GetString();
		
		return (strcmp(filter, entityName) == 0);
	}

	static bool IsBodyDestructionDebugFilterEnabled()
	{
		const char* filter = g_bodyDestruction_debugFilter->GetString();

		return ((strlen(filter) > 0) && (strcmp(filter, "0") != 0));
	}

private:
	static void Reload(IActor* pIActor);
	static void Reload(IEntity* pEntity);
	static void ReloadBodyDamage(IConsoleCmdArgs* pArgs);
	static void ReloadBodyDestruction(IConsoleCmdArgs* pArgs);

	static int g_bodyDamage_log;
	static int g_bodyDestruction_debug;

	static ICVar* g_bodyDestruction_debugFilter;
};

#endif