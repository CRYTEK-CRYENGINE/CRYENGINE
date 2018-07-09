// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// TODO: Can this live in Cry_Math.h?
template<class T>
inline T Lerp(const T& a, const T& b, float s)
{
	return T(a + s * (b - a));
}

//////////////////////////////////////////////////////////////////////////
//! Reports a Game Warning to validator with WARNING severity.
inline void FlowGraphWarning( const char *format,... ) PRINTF_PARAMS(1, 2);
inline void FlowGraphWarning( const char *format,... )
{
	if (!format)
		return;
	va_list args;
	va_start(args, format);
	GetISystem()->WarningV( VALIDATOR_MODULE_FLOWGRAPH,VALIDATOR_WARNING,0,NULL,format,args );
	va_end(args);
}

//////////////////////////////////////////////////////////////////////////
//! Reports a Game Warning to validator with WARNING severity.
inline void GameWarning(const char* format, ...)
{
	if (!format)
		return;

	va_list args;
	va_start(args, format);
	GetISystem()->WarningV(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, 0, 0, format, args);
	va_end(args);
}

//extern IGameTokenSystem *g_pGameTokenSystem;

inline IGameTokenSystem * GetIGameTokenSystem()
{
	return gEnv->pGameFramework->GetIGameTokenSystem();
}

inline IEntityAudioComponent* GetAudioProxy(EntityId id)
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (pEntity)
		return pEntity->GetOrCreateComponent<IEntityAudioComponent>();
	else
		return 0;
}

inline bool IsClientActor(EntityId id)
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (pEntity)
	{
		if (pEntity->GetFlags() & ENTITY_FLAG_LOCAL_PLAYER)
		{
			return true;
		}
	}
	return false;
}

//--------------------------------------------------------------
// returns true if the input entity is the local player. In single player, when the input entity is NULL, it returns true,  for backward compatibility
inline bool InputEntityIsLocalPlayer( const IFlowNode::SActivationInfo* const pActInfo )
{
	bool bRet = true;

	if (pActInfo->pEntity && (pActInfo->pEntity->GetFlags() & ENTITY_FLAG_LOCAL_PLAYER))
	{
		bRet = true;
	}
	else
	{
		if (gEnv->bMultiplayer)
			bRet = false;
	}

	return bRet;
}

//-------------------------------------------------------------
// returns the actor associated with the input entity. In single player, it returns the local player if that actor does not exists.
inline IActor* GetInputActor(const IFlowNode::SActivationInfo* const pActInfo)
{
	IActor* pActor = pActInfo->pEntity ? gEnv->pGameFramework->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId()) : nullptr;
	if (!pActor && !gEnv->bMultiplayer)
	{
		pActor = gEnv->pGameFramework->GetClientActor();
	}

	return pActor;
}
