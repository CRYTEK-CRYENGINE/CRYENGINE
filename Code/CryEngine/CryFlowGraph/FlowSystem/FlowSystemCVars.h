// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	CVars for the FlowSystem

   -------------------------------------------------------------------------
   History:
   - 02:03:2006  12:00 : Created by AlexL

*************************************************************************/

#ifndef __FLOWSYSTEM_CVARS_H__
#define __FLOWSYSTEM_CVARS_H__

#pragma once

struct ICVar;

struct CFlowSystemCVars
{
	int                             m_enableUpdates;    // Enable/Disable the whole FlowSystem
	int                             m_profile;          // Profile the FlowSystem
	int                             m_abortOnLoadError; // Abort execution when an error in FlowGraph loading is encountered
	int                             m_inspectorLog;     // Log inspector to console
	int                             m_noDebugText;      // Don't show debug text from HUD nodes (default 0)

	int                             m_enableFlowgraphNodeDebugging;
	int                             m_debugNextStep;

	// Disable HUD debug text
	int                             fg_DisableHUDText;

	int                             g_disableSequencePlayback;

	int                             g_disableInputKeyFlowNodeInDevMode;


	static inline CFlowSystemCVars& Get()
	{
		assert(s_pThis != 0);
		return *s_pThis;
	}

private:
	friend class CFlowSystem; // Our only creator

	CFlowSystemCVars(); // singleton stuff
	~CFlowSystemCVars();
	CFlowSystemCVars(const CFlowSystemCVars&);
	CFlowSystemCVars& operator=(const CFlowSystemCVars&);

	static CFlowSystemCVars* s_pThis;
};

#endif // __FLOWSYSTEM_CVARS_H__
