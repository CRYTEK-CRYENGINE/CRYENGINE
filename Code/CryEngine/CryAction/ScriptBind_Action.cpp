// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameObjects/GameObject.h"
#include "ScriptBind_Action.h"
#include "Serialization/XMLScriptLoader.h"
#include "CryAction.h"
#include "Network/GameServerChannel.h"
#include "Network/GameServerNub.h"
#include "Network/GameContext.h"
#include "SignalTimers/SignalTimers.h"
#include "RangeSignalingSystem/RangeSignaling.h"
#include <CryEntitySystem/IEntitySystem.h>
#include "AIProxy.h"
#include <CryGame/IGameTokens.h>
#include <IEffectSystem.h>
#include <IGameplayRecorder.h>
#include "PersistantDebug.h"
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIObjectManager.h>

//------------------------------------------------------------------------
CScriptBind_Action::CScriptBind_Action(CCryAction* pCryAction)
	: m_pCryAction(pCryAction)
{
	Init(gEnv->pScriptSystem, GetISystem());
	SetGlobalName("CryAction");

	RegisterGlobals();
	RegisterMethods();

	m_pPreviousView = NULL;
}

//------------------------------------------------------------------------
CScriptBind_Action::~CScriptBind_Action()
{
}

//------------------------------------------------------------------------
void CScriptBind_Action::RegisterGlobals()
{
	SCRIPT_REG_GLOBAL(eGE_DiscreetSample);
	SCRIPT_REG_GLOBAL(eGE_GameReset);
	SCRIPT_REG_GLOBAL(eGE_GameStarted);
	SCRIPT_REG_GLOBAL(eGE_SuddenDeath);
	SCRIPT_REG_GLOBAL(eGE_RoundEnd);
	SCRIPT_REG_GLOBAL(eGE_GameEnd);
	SCRIPT_REG_GLOBAL(eGE_Connected);
	SCRIPT_REG_GLOBAL(eGE_Disconnected);
	SCRIPT_REG_GLOBAL(eGE_Renamed);
	SCRIPT_REG_GLOBAL(eGE_ChangedTeam);
	SCRIPT_REG_GLOBAL(eGE_Death);
	SCRIPT_REG_GLOBAL(eGE_Scored);
	SCRIPT_REG_GLOBAL(eGE_Currency);
	SCRIPT_REG_GLOBAL(eGE_Rank);
	SCRIPT_REG_GLOBAL(eGE_Spectator);
	SCRIPT_REG_GLOBAL(eGE_ScoreReset);
	SCRIPT_REG_GLOBAL(eGE_Damage);
	SCRIPT_REG_GLOBAL(eGE_WeaponHit);

	gEnv->pScriptSystem->SetGlobalValue("QueryAimFromMovementController", CAIProxy::QueryAimFromMovementController);
	gEnv->pScriptSystem->SetGlobalValue("OverriddenAndAiming", CAIProxy::OverriddenAndAiming);
	gEnv->pScriptSystem->SetGlobalValue("OverriddenAndNotAiming", CAIProxy::OverriddenAndNotAiming);
}

//------------------------------------------------------------------------
void CScriptBind_Action::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Action::

	SCRIPT_REG_TEMPLFUNC(LoadXML, "definitionFile, dataFile");
	SCRIPT_REG_TEMPLFUNC(SaveXML, "definitionFile, dataFile, dataTable");
	SCRIPT_REG_TEMPLFUNC(IsServer, "");
	SCRIPT_REG_TEMPLFUNC(IsClient, "");
	SCRIPT_REG_TEMPLFUNC(IsGameStarted, "");
	SCRIPT_REG_TEMPLFUNC(IsRMIServer, "");
	SCRIPT_REG_TEMPLFUNC(IsGameObjectProbablyVisible, "entityId");
	SCRIPT_REG_TEMPLFUNC(GetPlayerList, "");
	SCRIPT_REG_TEMPLFUNC(ActivateEffect, "name");
	SCRIPT_REG_TEMPLFUNC(GetWaterInfo, "pos");
	SCRIPT_REG_TEMPLFUNC(GetServer, "number");
	SCRIPT_REG_TEMPLFUNC(ConnectToServer, "server");
	SCRIPT_REG_TEMPLFUNC(RefreshPings, "");
	SCRIPT_REG_TEMPLFUNC(GetServerTime, "");
	SCRIPT_REG_TEMPLFUNC(PauseGame, "pause");
	SCRIPT_REG_TEMPLFUNC(IsImmersivenessEnabled, "");
	SCRIPT_REG_TEMPLFUNC(IsChannelSpecial, "entityId/channelId");

	SCRIPT_REG_TEMPLFUNC(ForceGameObjectUpdate, "entityId, force");
	SCRIPT_REG_TEMPLFUNC(CreateGameObjectForEntity, "entityId");
	SCRIPT_REG_TEMPLFUNC(BindGameObjectToNetwork, "entityId");
	SCRIPT_REG_TEMPLFUNC(ActivateExtensionForGameObject, "entityId, extension, activate");
	SCRIPT_REG_TEMPLFUNC(SetNetworkParent, "entityId, parentId");
	SCRIPT_REG_TEMPLFUNC(IsChannelOnHold, "channelId");
	SCRIPT_REG_TEMPLFUNC(BanPlayer, "playerId, message");

	SCRIPT_REG_TEMPLFUNC(PersistantSphere, "pos, radius, color, name, timeout");
	SCRIPT_REG_TEMPLFUNC(PersistantLine, "start, end, color, name, timeout");
	SCRIPT_REG_TEMPLFUNC(PersistantArrow, "pos, radius, color, dir, name, timeout");
	SCRIPT_REG_TEMPLFUNC(Persistant2DText, "text, size, color, name, timeout");
	SCRIPT_REG_TEMPLFUNC(PersistantEntityTag, "entityId, text");
	SCRIPT_REG_TEMPLFUNC(ClearEntityTags, "entityId");
	SCRIPT_REG_TEMPLFUNC(ClearStaticTag, "entityId, staticId");

	SCRIPT_REG_TEMPLFUNC(SendGameplayEvent, "entityId, event, [desc], [value], [ID|ptr], [str]");

	SCRIPT_REG_TEMPLFUNC(CacheItemSound, "itemName");
	SCRIPT_REG_TEMPLFUNC(CacheItemGeometry, "itemName");

	SCRIPT_REG_TEMPLFUNC(DontSyncPhysics, "entityId");

	SCRIPT_REG_TEMPLFUNC(EnableSignalTimer, "entityId, text");
	SCRIPT_REG_TEMPLFUNC(DisableSignalTimer, "entityId, text");
	SCRIPT_REG_TEMPLFUNC(SetSignalTimerRate, "entityId, text, float, float");
	SCRIPT_REG_TEMPLFUNC(ResetSignalTimer, "entityId, text");

	SCRIPT_REG_TEMPLFUNC(EnableRangeSignaling, "entityId, bEnable");
	SCRIPT_REG_TEMPLFUNC(DestroyRangeSignaling, "entityId");
	SCRIPT_REG_TEMPLFUNC(ResetRangeSignaling, "entityId");

	SCRIPT_REG_TEMPLFUNC(AddRangeSignal, "entityId, float, float, text");
	SCRIPT_REG_TEMPLFUNC(AddTargetRangeSignal, "entityId, targetId, float, float, text");
	SCRIPT_REG_TEMPLFUNC(AddAngleSignal, "entityId, float, float, text");

	SCRIPT_REG_FUNC(SetViewCamera);
	SCRIPT_REG_FUNC(ResetToNormalCamera);

	SCRIPT_REG_FUNC(RegisterWithAI);
	SCRIPT_REG_TEMPLFUNC(HasAI, "entityId");

	SCRIPT_REG_TEMPLFUNC(GetClassName, "classId");
	SCRIPT_REG_TEMPLFUNC(SetAimQueryMode, "entityId, mode");

	SCRIPT_REG_TEMPLFUNC(PreLoadADB, "adbFileName");
}

int CScriptBind_Action::LoadXML(IFunctionHandler* pH, const char* definitionFile, const char* dataFile)
{
	return pH->EndFunction(XmlScriptLoad(definitionFile, dataFile));
}

int CScriptBind_Action::SaveXML(IFunctionHandler* pH, const char* definitionFile, const char* dataFile, SmartScriptTable dataTable)
{
	return pH->EndFunction(XmlScriptSave(definitionFile, dataFile, dataTable));
}

int CScriptBind_Action::IsGameStarted(IFunctionHandler* pH)
{
	return pH->EndFunction(m_pCryAction->IsGameStarted());
}

int CScriptBind_Action::IsRMIServer(IFunctionHandler* pH)
{
	return pH->EndFunction(m_pCryAction->GetGameServerNub() != 0);
}

int CScriptBind_Action::IsImmersivenessEnabled(IFunctionHandler* pH)
{
	int out = 0;
	if (!gEnv->bMultiplayer)
		out = 1;
	else if (CGameContext* pGC = CCryAction::GetCryAction()->GetGameContext())
		if (pGC->HasContextFlag(eGSF_ImmersiveMultiplayer))
			out = 1;
	return pH->EndFunction(out);
}

int CScriptBind_Action::IsChannelSpecial(IFunctionHandler* pH)
{
	INetChannel* pChannel = 0;

	if (pH->GetParamCount() > 0)
	{
		if (pH->GetParamType(1) == svtNumber)
		{
			int channelId = 0;
			if (pH->GetParam(1, channelId))
			{
				if (CGameServerChannel* pGameChannel = CCryAction::GetCryAction()->GetGameServerNub()->GetChannel(channelId))
					pChannel = pGameChannel->GetNetChannel();
			}
		}
		else if (pH->GetParamType(1) == svtPointer)
		{
			ScriptHandle entityId;
			if (pH->GetParam(1, entityId))
			{
				if (IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor((EntityId)entityId.n))
				{
					int channelId = pActor->GetChannelId();
					if (CGameServerChannel* pGameChannel = CCryAction::GetCryAction()->GetGameServerNub()->GetChannel(channelId))
						pChannel = pGameChannel->GetNetChannel();
				}
			}
		}

		if (pChannel && pChannel->IsPreordered())
			return pH->EndFunction(true);
	}

	return pH->EndFunction();
}

int CScriptBind_Action::IsClient(IFunctionHandler* pH)
{
	return pH->EndFunction(gEnv->IsClient());
}

int CScriptBind_Action::IsServer(IFunctionHandler* pH)
{
	return pH->EndFunction(gEnv->bServer);
}

int CScriptBind_Action::GetPlayerList(IFunctionHandler* pH)
{
	CGameServerNub* pNub = m_pCryAction->GetGameServerNub();
	if (!pNub)
	{
		GameWarning("No game server nub");
		return pH->EndFunction();
	}
	TServerChannelMap* playerMap = m_pCryAction->GetGameServerNub()->GetServerChannelMap();
	if (!playerMap)
		return pH->EndFunction();

	IEntitySystem* pES = gEnv->pEntitySystem;

	int k = 1;
	SmartScriptTable playerList(m_pSS);

	for (TServerChannelMap::iterator it = playerMap->begin(); it != playerMap->end(); ++it)
	{
		EntityId playerId = it->second->GetPlayerId();

		if (!playerId)
			continue;

		IEntity* pPlayer = pES->GetEntity(playerId);
		if (!pPlayer)
			continue;
		if (pPlayer->GetScriptTable())
			playerList->SetAt(k++, pPlayer->GetScriptTable());
	}

	return pH->EndFunction(*playerList);
}

int CScriptBind_Action::IsGameObjectProbablyVisible(IFunctionHandler* pH, ScriptHandle gameObject)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)gameObject.n);
	if (pEntity)
	{
		CGameObject* pGameObject = static_cast<CGameObject*>(pEntity->GetProxy(ENTITY_PROXY_USER));
		if (pGameObject && pGameObject->IsProbablyVisible())
			return pH->EndFunction(1);
	}
	return pH->EndFunction();
}

int CScriptBind_Action::ActivateEffect(IFunctionHandler* pH, const char* name)
{
	int i = CCryAction::GetCryAction()->GetIEffectSystem()->GetEffectId(name);
	CCryAction::GetCryAction()->GetIEffectSystem()->Activate(i);
	return pH->EndFunction();
}

int CScriptBind_Action::GetWaterInfo(IFunctionHandler* pH, Vec3 pos)
{
	const int mb = 8;
	pe_params_buoyancy buoyancy[mb];
	Vec3 gravity;

	if (int n = gEnv->pPhysicalWorld->CheckAreas(pos, gravity, buoyancy, mb))
	{
		for (int i = 0; i < n; i++)
		{
			if (buoyancy[i].iMedium == 0) // 0==water
			{
				return pH->EndFunction(buoyancy[i].waterPlane.origin.z,
				                       Script::SetCachedVector(buoyancy[i].waterPlane.n, pH, 2), Script::SetCachedVector(buoyancy[i].waterFlow, pH, 2));
			}
		}
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::GetServer(IFunctionHandler* pFH, int number)
{
	char* server = 0;
	int ping = 9999;
	char* data = 0;

	ILanQueryListener* pLanQueryListener = m_pCryAction->GetILanQueryListener();
	IGameQueryListener* pGameQueryListener = NULL;
	if (pLanQueryListener)
		pGameQueryListener = pLanQueryListener->GetGameQueryListener();
	if (pGameQueryListener)
		pGameQueryListener->GetServer(number, &server, &data, ping);

	return pFH->EndFunction(server, data, ping);
}

//------------------------------------------------------------------------
int CScriptBind_Action::RefreshPings(IFunctionHandler* pFH)
{
	ILanQueryListener* pLanQueryListener = m_pCryAction->GetILanQueryListener();
	IGameQueryListener* pGameQueryListener = NULL;
	if (pLanQueryListener)
		pGameQueryListener = pLanQueryListener->GetGameQueryListener();
	if (pGameQueryListener)
		pGameQueryListener->RefreshPings();
	return pFH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::ConnectToServer(IFunctionHandler* pFH, char* server)
{
	ILanQueryListener* pLanQueryListener = m_pCryAction->GetILanQueryListener();
	if (pLanQueryListener)
		pLanQueryListener->GetGameQueryListener()->ConnectToServer(server);
	return pFH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::GetServerTime(IFunctionHandler* pFH)
{
	return pFH->EndFunction(m_pCryAction->GetServerTime().GetSeconds());
}

//------------------------------------------------------------------------
int CScriptBind_Action::ForceGameObjectUpdate(IFunctionHandler* pH, ScriptHandle entityId, bool force)
{
	if (IGameObject* pGameObject = CCryAction::GetCryAction()->GetGameObject((EntityId)entityId.n))
		pGameObject->ForceUpdate(force);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::CreateGameObjectForEntity(IFunctionHandler* pH, ScriptHandle entityId)
{
	CCryAction::GetCryAction()->GetIGameObjectSystem()->CreateGameObjectForEntity((EntityId)entityId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::BindGameObjectToNetwork(IFunctionHandler* pH, ScriptHandle entityId)
{
	if (IGameObject* pGameObject = CCryAction::GetCryAction()->GetGameObject((EntityId)entityId.n))
		pGameObject->BindToNetwork();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::ActivateExtensionForGameObject(IFunctionHandler* pH, ScriptHandle entityId, const char* extension, bool activate)
{
	if (IGameObject* pGameObject = CCryAction::GetCryAction()->GetGameObject((EntityId)entityId.n))
	{
		if (activate)
			pGameObject->ActivateExtension(extension);
		else
			pGameObject->DeactivateExtension(extension);
	}

	return pH->EndFunction();

}

//------------------------------------------------------------------------
int CScriptBind_Action::SetNetworkParent(IFunctionHandler* pH, ScriptHandle entityId, ScriptHandle parentId)
{
	if (IGameObject* pGameObject = CCryAction::GetCryAction()->GetGameObject((EntityId)entityId.n))
		pGameObject->SetNetworkParent((EntityId)parentId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::IsChannelOnHold(IFunctionHandler* pH, int channelId)
{
	if (CGameServerChannel* pServerChannel = CCryAction::GetCryAction()->GetGameServerNub()->GetChannel(channelId))
		return pH->EndFunction(pServerChannel->IsOnHold());
	return pH->EndFunction();
}

int CScriptBind_Action::BanPlayer(IFunctionHandler* pH, ScriptHandle entityId, const char* message)
{
	if (IActor* pAct = CCryAction::GetCryAction()->GetIActorSystem()->GetActor((EntityId)entityId.n))
	{
		CCryAction::GetCryAction()->GetGameServerNub()->BanPlayer(pAct->GetChannelId(), message);
	}
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::PauseGame(IFunctionHandler* pH, bool pause)
{
	bool forced = false;

	if (pH->GetParamCount() > 1)
	{
		pH->GetParam(2, forced);
	}
	CCryAction::GetCryAction()->PauseGame(pause, forced);

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////
int CScriptBind_Action::SetViewCamera(IFunctionHandler* pH)
{

	// save previous valid view
	IView* pView = m_pCryAction->GetIViewSystem()->GetActiveView();
	if (pView && !m_pPreviousView)
		m_pPreviousView = pView;

	// override view with our camera settings.
	pView = NULL;
	m_pCryAction->GetIViewSystem()->SetActiveView(pView);

	Vec3 vPos(0, 0, 0);
	Vec3 vDir(0, 0, 0);

	pH->GetParam(1, vPos);
	pH->GetParam(2, vDir);

	CCamera camera(GetISystem()->GetViewCamera());
	float fRoll(camera.GetAngles().z);

	camera.SetMatrix(CCamera::CreateOrientationYPR(CCamera::CreateAnglesYPR(vDir, fRoll)));
	camera.SetPosition(vPos);
	GetISystem()->SetViewCamera(camera);

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////
int CScriptBind_Action::ResetToNormalCamera(IFunctionHandler* pH)
{
	if (m_pPreviousView)
	{
		m_pCryAction->GetIViewSystem()->SetActiveView(m_pPreviousView);
		m_pPreviousView = nullptr;
	}
	return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Action::PersistantSphere(IFunctionHandler* pH, Vec3 pos, float radius, Vec3 color, const char* name, float timeout)
{
	IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();

	pPD->Begin(name, false);
	pPD->AddSphere(pos, radius, ColorF(color, 1.f), timeout);

	return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Action::PersistantLine(IFunctionHandler* pH, Vec3 start, Vec3 end, Vec3 color, const char* name, float timeout)
{
	IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();

	pPD->Begin(name, false);
	pPD->AddLine(start, end, ColorF(color, 1.f), timeout);

	return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Action::PersistantArrow(IFunctionHandler* pH, Vec3 pos, float radius, Vec3 dir, Vec3 color, const char* name, float timeout)
{
	IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();

	pPD->Begin(name, false);
	pPD->AddDirection(pos, radius, dir, ColorF(color, 1.f), timeout);

	return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Action::Persistant2DText(IFunctionHandler* pH, const char* text, float size, Vec3 color, const char* name, float timeout)
{
	IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();

	pPD->Begin(name, false);
	pPD->Add2DText(text, size, ColorF(color, 1.f), timeout);

	return pH->EndFunction();
}

//-------------------------------------------------------------------------
// Required Params: entityId, const char *text
// Optional Params: float size, Vec3 color, float visibleTime, float fadeTime, float viewDistance, const char* staticId, int columnNum, const char* contextTag
int CScriptBind_Action::PersistantEntityTag(IFunctionHandler* pH, ScriptHandle entityId, const char* text)
{
	IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();

	SEntityTagParams params;
	params.entity = (EntityId)entityId.n;
	params.text = text;
	params.tagContext = "scriptbind";

	// Optional params
	if (pH->GetParamType(3) != svtNull) // Size
		pH->GetParam(3, params.size);
	if (pH->GetParamType(4) != svtNull) // Color
	{
		Vec3 color;
		pH->GetParam(4, color);
		params.color = ColorF(color, 1.f);
	}
	if (pH->GetParamType(5) != svtNull) // Visible Time
		pH->GetParam(5, params.visibleTime);
	if (pH->GetParamType(6) != svtNull) // Fade Time
		pH->GetParam(6, params.fadeTime);
	if (pH->GetParamType(7) != svtNull) // View Distance
		pH->GetParam(7, params.viewDistance);
	if (pH->GetParamType(8) != svtNull) // Static ID
	{
		const char* staticId;
		pH->GetParam(8, staticId);
		params.staticId = staticId;
	}
	if (pH->GetParamType(9) != svtNull) // Column Num
		pH->GetParam(9, params.column);
	if (pH->GetParamType(10) != svtNull) // Context Tag
	{
		const char* tagContext;
		pH->GetParam(10, tagContext);
		params.tagContext = tagContext; // overrides default one set above
	}

	pPD->AddEntityTag(params);

	return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Action::ClearEntityTags(IFunctionHandler* pH, ScriptHandle entityId)
{
	IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();
	pPD->ClearEntityTags((EntityId)entityId.n);

	return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Action::ClearStaticTag(IFunctionHandler* pH, ScriptHandle entityId, const char* staticId)
{
	IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();
	pPD->ClearStaticTag((EntityId)entityId.n, staticId);

	return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Action::EnableSignalTimer(IFunctionHandler* pH, ScriptHandle entityId, const char* sText)
{
	bool bRet = CSignalTimer::ref().EnablePersonalManager((EntityId)entityId.n, sText);
	return pH->EndFunction(bRet);
}

//-------------------------------------------------------------------------
int CScriptBind_Action::DisableSignalTimer(IFunctionHandler* pH, ScriptHandle entityId, const char* sText)
{
	bool bRet = CSignalTimer::ref().DisablePersonalSignalTimer((EntityId)entityId.n, sText);
	return pH->EndFunction(bRet);
}

//-------------------------------------------------------------------------
int CScriptBind_Action::SetSignalTimerRate(IFunctionHandler* pH, ScriptHandle entityId, const char* sText, float fRateMin, float fRateMax)
{
	bool bRet = CSignalTimer::ref().SetTurnRate((EntityId)entityId.n, sText, fRateMin, fRateMax);
	return pH->EndFunction(bRet);
}

//-------------------------------------------------------------------------
int CScriptBind_Action::ResetSignalTimer(IFunctionHandler* pH, ScriptHandle entityId, const char* sText)
{
	bool bRet = CSignalTimer::ref().ResetPersonalTimer((EntityId)entityId.n, sText);
	return pH->EndFunction(bRet);
}

//------------------------------------------------------------------------
int CScriptBind_Action::SendGameplayEvent(IFunctionHandler* pH, ScriptHandle entityId, int event)
{
	const char* desc = 0;
	float value = 0.0f;
	ScriptHandle hdl;

	const char* str_data = 0;

	if (pH->GetParamCount() > 2 && pH->GetParamType(3) == svtString)
		pH->GetParam(3, desc);
	if (pH->GetParamCount() > 3 && pH->GetParamType(4) == svtNumber)
		pH->GetParam(4, value);
	if (pH->GetParamCount() > 4 && pH->GetParamType(5) == svtPointer)
		pH->GetParam(5, hdl);
	if (pH->GetParamCount() > 5 && pH->GetParamType(6) == svtString)
		pH->GetParam(6, str_data);

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n);
	CCryAction::GetCryAction()->GetIGameplayRecorder()->Event(pEntity, GameplayEvent(event, desc, value, hdl.ptr, str_data));

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::CacheItemSound(IFunctionHandler* pH, const char* itemName)
{
	CCryAction::GetCryAction()->GetIItemSystem()->CacheItemSound(itemName);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::CacheItemGeometry(IFunctionHandler* pH, const char* itemName)
{
	CCryAction::GetCryAction()->GetIItemSystem()->CacheItemGeometry(itemName);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::EnableRangeSignaling(IFunctionHandler* pH, ScriptHandle entityId, bool bEnable)
{
	CCryAction::GetCryAction()->GetRangeSignaling()->EnablePersonalRangeSignaling((EntityId)entityId.n, bEnable);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::DestroyRangeSignaling(IFunctionHandler* pH, ScriptHandle entityId)
{
	CCryAction::GetCryAction()->GetRangeSignaling()->DestroyPersonalRangeSignaling((EntityId)entityId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::ResetRangeSignaling(IFunctionHandler* pH, ScriptHandle entityId)
{
	CCryAction::GetCryAction()->GetRangeSignaling()->ResetPersonalRangeSignaling((EntityId)entityId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::AddRangeSignal(IFunctionHandler* pH, ScriptHandle entityId, float fRadius, float fFlexibleBoundary, const char* sSignal)
{
	if (gEnv->pAISystem)
	{
		// Get optional signal data
		IAISignalExtraData* pData = NULL;
		if (pH->GetParamCount() > 4)
		{
			SmartScriptTable dataTable;
			if (pH->GetParam(5, dataTable))
			{
				pData = gEnv->pAISystem->CreateSignalExtraData();
				CRY_ASSERT(pData);
				pData->FromScriptTable(dataTable);
			}
		}

		CCryAction::GetCryAction()->GetRangeSignaling()->AddRangeSignal((EntityId)entityId.n, fRadius, fFlexibleBoundary, sSignal, pData);
		gEnv->pAISystem->FreeSignalExtraData(pData);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::AddTargetRangeSignal(IFunctionHandler* pH, ScriptHandle entityId, ScriptHandle targetId, float fRadius, float fFlexibleBoundary, const char* sSignal)
{
	if (gEnv->pAISystem)
	{
		// Get optional signal data
		IAISignalExtraData* pData = NULL;
		if (pH->GetParamCount() > 5)
		{
			SmartScriptTable dataTable;
			if (pH->GetParam(6, dataTable))
			{
				pData = gEnv->pAISystem->CreateSignalExtraData();
				CRY_ASSERT(pData);
				pData->FromScriptTable(dataTable);
			}
		}

		CCryAction::GetCryAction()->GetRangeSignaling()->AddTargetRangeSignal((EntityId)entityId.n, (EntityId)targetId.n, fRadius, fFlexibleBoundary, sSignal, pData);
		gEnv->pAISystem->FreeSignalExtraData(pData);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::AddAngleSignal(IFunctionHandler* pH, ScriptHandle entityId, float fAngle, float fFlexibleBoundary, const char* sSignal)
{
	if (gEnv->pAISystem)
	{
		// Get optional signal data
		IAISignalExtraData* pData = NULL;
		if (pH->GetParamCount() > 4)
		{
			SmartScriptTable dataTable;
			if (pH->GetParam(5, dataTable))
			{
				pData = gEnv->pAISystem->CreateSignalExtraData();
				CRY_ASSERT(pData);
				pData->FromScriptTable(dataTable);
			}
		}

		CCryAction::GetCryAction()->GetRangeSignaling()->AddAngleSignal((EntityId)entityId.n, fAngle, fFlexibleBoundary, sSignal, pData);
		gEnv->pAISystem->FreeSignalExtraData(pData);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::DontSyncPhysics(IFunctionHandler* pH, ScriptHandle entityId)
{
	if (CGameObject* pGO = static_cast<CGameObject*>(CCryAction::GetCryAction()->GetIGameObjectSystem()->CreateGameObjectForEntity((EntityId)entityId.n)))
		pGO->DontSyncPhysics();
	else
		GameWarning("DontSyncPhysics: Unable to find entity %" PRISIZE_T, entityId.n);
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
// (MATT) Moved here from Scriptbind_AI when that was moved to the AI system {2008/02/15:15:23:16}
int CScriptBind_Action::RegisterWithAI(IFunctionHandler* pH)
{
	if (gEnv->bMultiplayer && !gEnv->bServer)
		return pH->EndFunction();

	int type;
	ScriptHandle hdl;

	if (!pH->GetParams(hdl, type))
		return pH->EndFunction();

	EntityId entityID = (EntityId)hdl.n;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityID);

	if (!pEntity)
	{
		GameWarning("RegisterWithAI: Tried to set register with AI nonExisting entity with id [%d]. ", entityID);
		return pH->EndFunction();
	}

	// Apparently we can't assume that there is just one IGameObject to an entity, because we choose between (at least) Actor and Vehicle objects.
	// (MATT) Do we really need to check on the actor system here? {2008/02/15:18:38:34}
	IGameFramework* pGameFramework = gEnv->pGameFramework;
	IVehicleSystem* pVSystem = pGameFramework->GetIVehicleSystem();
	IActorSystem* pASystem = pGameFramework->GetIActorSystem();
	if (!pASystem)
	{
		GameWarning("RegisterWithAI: no ActorSystem for %s.", pEntity->GetName());
		return pH->EndFunction();
	}

	AIObjectParams params(type, 0, entityID);
	bool autoDisable(true);

	// For most types, we need to parse the tables
	// For others we leave them blank
	switch (type)
	{
	case AIOBJECT_ACTOR:
	case AIOBJECT_2D_FLY:
	case AIOBJECT_BOAT:
	case AIOBJECT_CAR:
	case AIOBJECT_HELICOPTER:

	case AIOBJECT_INFECTED:
	case AIOBJECT_ALIENTICK:

	case AIOBJECT_HELICOPTERCRYSIS2:
		if (gEnv->pAISystem && !gEnv->pAISystem->ParseTables(3, true, pH, params, autoDisable))
			return pH->EndFunction();
	default:
		;
	}

	// Most types check these, so just get them in advance
	IActor* pActor = pASystem->GetActor(pEntity->GetId());

	IVehicle* pVehicle = NULL;
	if (pVSystem)
		pVehicle = pVSystem->GetVehicle(pEntity->GetId());

	// Set this if we've found something to create a proxy from
	IGameObject* pGameObject = NULL;

	switch (type)
	{
	case AIOBJECT_ACTOR:
	case AIOBJECT_2D_FLY:

	case AIOBJECT_INFECTED:
	case AIOBJECT_ALIENTICK:
		{
			// (MATT) The pActor/pVehicle test below - is it basically trying to distiguish between the two cases above? If so, separate them! {2008/02/15:19:38:08}
			if (!pActor)
			{
				GameWarning("RegisterWithAI: no Actor for %s.", pEntity->GetName());
				return pH->EndFunction();
			}
			pGameObject = pActor->GetGameObject();
		}
		break;
	case AIOBJECT_BOAT:
	case AIOBJECT_CAR:
		{
			if (!pVehicle)
			{
				GameWarning("RegisterWithAI: no Vehicle for %s (Id %i).", pEntity->GetName(), pEntity->GetId());
				return pH->EndFunction();
			}
			pGameObject = pVehicle->GetGameObject();
		}
		break;

	case AIOBJECT_HELICOPTER:
	case AIOBJECT_HELICOPTERCRYSIS2:
		{
			if (!pVehicle)
			{
				GameWarning("RegisterWithAI: no Vehicle for %s (Id %i).", pEntity->GetName(), pEntity->GetId());
				return pH->EndFunction();
			}
			pGameObject = pVehicle->GetGameObject();
			params.m_moveAbility.b3DMove = true;
		}
		break;
	case AIOBJECT_PLAYER:
		{
			if (IsDemoPlayback())
				return pH->EndFunction();

			SmartScriptTable pTable;

			if (pH->GetParamCount() > 2)
				pH->GetParam(3, pTable);
			else
				return pH->EndFunction();

			pGameObject = pActor->GetGameObject();

			pTable->GetValue("groupid", params.m_sParamStruct.m_nGroup);

			const char* faction = 0;
			if (pTable->GetValue("esFaction", faction) && gEnv->pAISystem)
			{
				params.m_sParamStruct.factionID = gEnv->pAISystem->GetFactionMap().GetFactionID(faction);
				if (faction && *faction && (params.m_sParamStruct.factionID == IFactionMap::InvalidFactionID))
				{
					GameWarning("Unknown faction '%s' being set...", faction);
				}
			}
			else
			{
				// Márcio: backwards compatibility
				int species = -1;
				if (!pTable->GetValue("eiSpecies", species))
					pTable->GetValue("species", species);

				if (species > -1)
					params.m_sParamStruct.factionID = species;
			}

			pTable->GetValue("commrange", params.m_sParamStruct.m_fCommRange); //Luciano - added to use GROUPONLY signals

			SmartScriptTable pPerceptionTable;
			if (pTable->GetValue("Perception", pPerceptionTable))
			{
				pPerceptionTable->GetValue("sightrange", params.m_sParamStruct.m_PerceptionParams.sightRange);
			}
		}
		break;
	case AIOBJECT_SNDSUPRESSOR:
		{
			// (MATT) This doesn't need a proxy? {2008/02/15:19:45:58}
			SmartScriptTable pTable;
			// Properties table
			if (pH->GetParamCount() > 2)
				pH->GetParam(3, pTable);
			else
				return pH->EndFunction();
			if (!pTable->GetValue("radius", params.m_moveAbility.pathRadius))
				params.m_moveAbility.pathRadius = 10.f;
			break;
		}
	case AIOBJECT_WAYPOINT:
		break;
		/*
		   // this block is commented out since params.m_sParamStruct is currently ignored in pEntity->RegisterInAISystem()
		   // instead of setting the group id here, it will be set from the script right after registering
		   default:
		   // try to get groupid settings for anchors
		   params.m_sParamStruct.m_nGroup = -1;
		   params.m_sParamStruct.m_nSpecies = -1;
		   {
		   SmartScriptTable pTable;
		   if ( pH->GetParamCount() > 2 )
		   pH->GetParam( 3, pTable );
		   if ( *pTable )
		   pTable->GetValue( "groupid", params.m_sParamStruct.m_nGroup );
		   }
		   break;
		 */
	}

	params.name = pEntity->GetName();

	// Register in AI to get a new AI object, deregistering the old one in the process
	gEnv->pAISystem->GetAIObjectManager()->CreateAIObject(params);

	// (MATT) ? {2008/02/15:19:46:29}
	// AI object was not created (possibly AI System is disabled)
	if (IAIObject* aiObject = pEntity->GetAI())
	{
		if (type == AIOBJECT_SNDSUPRESSOR)
			aiObject->SetRadius(params.m_moveAbility.pathRadius);
		else if (type >= AIANCHOR_FIRST) // if anchor - set radius
		{
			SmartScriptTable pTable;
			// Properties table
			if (pH->GetParamCount() > 2)
				pH->GetParam(3, pTable);
			else
				return pH->EndFunction();
			float radius(0.f);
			pTable->GetValue("radius", radius);
			int groupId = -1;
			pTable->GetValue("groupid", groupId);
			aiObject->SetGroupId(groupId);
			aiObject->SetRadius(radius);
		}

		if (IAIActorProxy* proxy = aiObject->GetProxy())
			proxy->UpdateMeAlways(!autoDisable);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::HasAI(IFunctionHandler* pH, ScriptHandle entityId)
{
	bool bResult = false;

	const EntityId id = (EntityId)entityId.n;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (pEntity)
	{
		bResult = (pEntity->GetAI() != 0);
	}

	return pH->EndFunction(bResult);
}

//------------------------------------------------------------------------
int CScriptBind_Action::GetClassName(IFunctionHandler* pH, int classId)
{
	char className[128];
	bool retrieved = CCryAction::GetCryAction()->GetNetworkSafeClassName(className, sizeof(className), classId);

	if (retrieved)
	{
		return pH->EndFunction(className);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::SetAimQueryMode(IFunctionHandler* pH, ScriptHandle entityId, int mode)
{
	IEntity* entity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityId.n));
	IAIObject* ai = entity ? entity->GetAI() : NULL;
	CAIProxy* proxy = ai ? static_cast<CAIProxy*>(ai->GetProxy()) : NULL;

	if (proxy)
		proxy->SetAimQueryMode(static_cast<CAIProxy::AimQueryMode>(mode));

	return pH->EndFunction();
}

// ============================================================================
//	Pre-load a mannequin ADB file.
//
//	In:		The function handler (NULL is invalid!)
//	In:		The name of the ADB file.
//
//	Returns:	A default result code (in Lua: void).
//
int CScriptBind_Action::PreLoadADB(IFunctionHandler* pH, const char* adbFileName)
{
	IF_LIKELY (adbFileName != NULL)
	{
		IMannequin& mannequinInterface = gEnv->pGameFramework->GetMannequinInterface();
		if (mannequinInterface.GetAnimationDatabaseManager().Load(adbFileName) == NULL)
		{
			GameWarning("PreLoadADB(): Failed to pre-load '%s'!", adbFileName);
		}
	}

	return pH->EndFunction();
}
