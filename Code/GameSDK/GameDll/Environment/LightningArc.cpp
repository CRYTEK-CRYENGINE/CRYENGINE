// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LightningArc.h"
#include "ScriptBind_LightningArc.h"
#include "Effects/GameEffects/LightningGameEffect.h"
#include "Effects/RenderNodes/LightningNode.h"
#include "EntityUtility/EntityScriptCalls.h"



void CLightningArc::GetMemoryUsage(ICrySizer *pSizer) const {}
void CLightningArc::PostInit( IGameObject * pGameObject ) {}
void CLightningArc::InitClient(int channelId) {}
void CLightningArc::PostInitClient(int channelId) {}
bool CLightningArc::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {return true;}
void CLightningArc::PostReloadExtension(IGameObject * pGameObject, const SEntitySpawnParams &params) {}
bool CLightningArc::GetEntityPoolSignature(TSerialize signature) {return true;}
void CLightningArc::FullSerialize(TSerialize ser) {}
bool CLightningArc::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags) {return true;}
void CLightningArc::PostSerialize() {}
void CLightningArc::SerializeSpawnInfo(TSerialize ser) {}
ISerializableInfoPtr CLightningArc::GetSpawnInfo() {return ISerializableInfoPtr();}
void CLightningArc::HandleEvent( const SGameObjectEvent& event ) {}
void CLightningArc::SetChannelId(uint16 id) {}
const void* CLightningArc::GetRMIBase() const {return 0;}
void CLightningArc::PostUpdate(float frameTime) {}
void CLightningArc::PostRemoteSpawn() {}



CLightningArc::CLightningArc()
	:	m_enabled(true)
	,	m_delay(5.0f)
	,	m_delayVariation(0.0f)
	,	m_timer(0.0f)
	,	m_inGameMode(!gEnv->IsEditor())
	, m_lightningPreset(NULL)
{
}



bool CLightningArc::Init(IGameObject* pGameObject)
{
	SetGameObject(pGameObject);

	g_pGame->GetLightningArcScriptBind()->AttachTo(this);

	ReadLuaParameters();

	return true;
}



void CLightningArc::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
		case ENTITY_EVENT_LEVEL_LOADED:
			Enable(m_enabled);
			break;
		case ENTITY_EVENT_RESET:
			Reset(event.nParam[0]!=0);
			break;
	}
}

uint64 CLightningArc::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_LEVEL_LOADED) | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET);
}

void CLightningArc::Update(SEntityUpdateContext& ctx, int updateSlot)
{
	if (!m_enabled || !m_inGameMode)
		return;

	m_timer -= ctx.fFrameTime;

	if (m_timer < 0.0f)
	{
		TriggerSpark();
		m_timer += m_delay + cry_random(0.5f, 1.0f) * m_delayVariation;
	}
}



void CLightningArc::TriggerSpark()
{
	const char* targetName = "Target";
	IEntity* pEntity = GetEntity();

	if (pEntity->GetMaterial() == 0)
	{
		GameWarning("Lightning arc '%s' has no Material, no sparks will trigger", pEntity->GetName());
		return;
	}

	IEntityLink* pLinks = pEntity->GetEntityLinks();
	int numLinks = 0;
	for (IEntityLink* link = pLinks; link; link = link->next)
	{
		if (strcmp(link->name, targetName) == 0)
			++numLinks;
	}

	if (numLinks == 0)
	{
		GameWarning("Lightning arc '%s' has no Targets, no sparks will trigger", pEntity->GetName());
		return;
	}

	int nextSpark = cry_random(0, numLinks - 1);
	IEntityLink* pNextSparkLink = pLinks;
	for (; nextSpark && pNextSparkLink; pNextSparkLink = pNextSparkLink->next)
	{
		if (strcmp(pNextSparkLink->name, targetName) == 0)
			--nextSpark;
	}

	assert(pNextSparkLink);
	PREFAST_ASSUME(pNextSparkLink);

	CLightningGameEffect::TIndex id = g_pGame->GetLightningGameEffect()->TriggerSpark(
		m_lightningPreset,
		pEntity->GetMaterial(),
		CLightningGameEffect::STarget(GetEntityId()),
		CLightningGameEffect::STarget(pNextSparkLink->entityId));
	float strikeTime = g_pGame->GetLightningGameEffect()->GetSparkRemainingTime(id);
	IScriptTable* pTargetScriptTable = 0;
	IEntity* pTarget = pNextSparkLink ? gEnv->pEntitySystem->GetEntity(pNextSparkLink->entityId) : NULL;
	if (pTarget)
		pTargetScriptTable = pTarget->GetScriptTable();

	EntityScripts::CallScriptFunction(pEntity, pEntity->GetScriptTable(), "OnStrike", strikeTime, pTargetScriptTable);
}



void CLightningArc::Enable(bool enable)
{
	if (m_enabled != enable)
		m_timer = 0.0f;
	m_enabled = enable;

	if (m_enabled && m_inGameMode)
		GetGameObject()->EnableUpdateSlot(this, 0);
	else
		GetGameObject()->DisableUpdateSlot(this, 0);
}



void CLightningArc::Reset(bool jumpingIntoGame)
{
	m_inGameMode = jumpingIntoGame;
	ReadLuaParameters();
}



void CLightningArc::ReadLuaParameters()
{
	SmartScriptTable pScriptTable = GetEntity()->GetScriptTable();
	if (!pScriptTable)
		return;

	SmartScriptTable pProperties;
	SmartScriptTable pTiming;
	SmartScriptTable pRender;
	if (!pScriptTable->GetValue("Properties", pProperties))
		return;
	if (!pProperties->GetValue("Timing", pTiming))
		return;
	if (!pProperties->GetValue("Render", pRender))
		return;

	pProperties->GetValue("bActive", m_enabled);
	Enable(m_enabled);

	pTiming->GetValue("fDelay", m_delay);
	pTiming->GetValue("fDelayVariation", m_delayVariation);

	pRender->GetValue("ArcPreset", m_lightningPreset);
}



void CLightningArc::Release()
{
	delete this;
}
