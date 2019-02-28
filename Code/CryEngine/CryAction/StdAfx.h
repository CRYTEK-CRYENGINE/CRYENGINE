// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   CryGame Source File.
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	include file for standard system include files,	or project
                specific include files that are used frequently, but are
                changed infrequently

   -------------------------------------------------------------------------
   History:
   - 20:7:2004   10:51 : Created by Marco Koegler

*************************************************************************/
#if !defined(AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E42__INCLUDED_)
#define AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E42__INCLUDED_

//#define _CRTDBG_MAP_ALLOC

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule  eCryM_GameFramework
#define RWI_NAME_TAG "RayWorldIntersection(Action)"
#define PWI_NAME_TAG "PrimitiveWorldIntersection(Action)"

#define CRYACTION_EXPORTS

// Insert your headers here
#include <CryCore/Platform/platform.h>

#include <memory>
#include <vector>
#include <map>
#include <queue>

inline void GameWarning(const char*, ...) PRINTF_PARAMS(1, 2);

#include <CryCore/Project/ProjectDefines.h>
#include <CryCore/StlUtils.h>
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Camera.h>
#include <CryMath/Random.h>
#include <CrySystem/ISystem.h>
#include <CryNetwork/INetwork.h>
#include <CryInput/IInput.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryNetwork/NetHelpers.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/ILog.h>
#include <CryNetwork/IRemoteControl.h>
#include <CryNetwork/ISimpleHttpServer.h>
#include <CryAction.h>
#include <CryGame/IGameFramework.h>
#include <IActorSystem.h>
#include <IAnimatedCharacter.h>
#include <CryGame/IGame.h>
#include <IItem.h>
#include <IItemSystem.h>
#include <IViewSystem.h>
#include <IVehicleSystem.h>
#include <CryFlowGraph/IFlowSystem.h>
#include <IGameplayRecorder.h>
#include <CryGame/GameUtils.h>

#pragma warning(disable: 4018)  // conditional expression is constant
#pragma warning(disable: 4018)  // conditional expression is constant
#pragma warning(disable: 4503)  // decorated name length exceeded, name was truncated

#if !defined(_RELEASE)
	#define ENABLE_NETDEBUG 1
#endif

//////////////////////////////////////////////////////////////////////////
//! Reports a Game Warning to validator with WARNING severity.
inline void GameWarning(const char* format, ...)
{
	if (!format)
		return;

	va_list args;
	va_start(args, format);
	GetISystem()->WarningV(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, 0, 0, format, args);
	va_end(args);
}

#if 1
	#define NET_USE_SIMPLE_BREAKAGE 1
#else// deprecated and won't compile
	#define NET_USE_SIMPLE_BREAKAGE 0
#endif

#if !defined(RELEASE)
	#define CRYACTION_AI_VERBOSITY
#endif

#ifdef CRYACTION_AI_VERBOSITY
	#define AIWarningID     gEnv->pAISystem->Warning
	#define AIErrorID       gEnv->pAISystem->Error
	#define AILogProgressID gEnv->pAISystem->LogProgress
	#define AILogEventID    gEnv->pAISystem->LogEvent
	#define AILogCommentID  gEnv->pAISystem->LogComment
#else
	#define AIWarningID     (void)
	#define AIErrorID       (void)
	#define AILogProgressID (void)
	#define AILogEventID    (void)
	#define AILogCommentID  (void)
#endif

inline bool IsClientActor(EntityId id)
{
	IActor* pActor = CCryAction::GetCryAction()->GetClientActor();
	if (pActor && pActor->GetEntity()->GetId() == id)
		return true;
	return false;
}

template<typename T> bool inline GetAttr(const XmlNodeRef& node, const char* key, T& val)
{
	return node->getAttr(key, val);
}

bool inline GetTimeAttr(const XmlNodeRef& node, const char* key, time_t& val)
{
	const char* pVal = node->getAttr(key);
	if (!pVal)
		return false;
	val = GameUtils::stringToTime(pVal);
	return true;
}

template<> bool inline GetAttr(const XmlNodeRef& node, const char* key, string& val)
{
	const char* pVal = node->getAttr(key);
	if (!pVal)
		return false;
	val = pVal;
	return true;
}

#endif // !defined(AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E42__INCLUDED_)
