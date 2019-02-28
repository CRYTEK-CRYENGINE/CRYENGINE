// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ScriptProxy.h"
#include "EntityScript.h"
#include "Entity.h"
#include "ScriptProperties.h"
#include "ScriptBind_Entity.h"
#include "EntitySystem.h"
#include <CryNetwork/ISerialize.h>
#include <CryNetwork/INetwork.h>
#include "EntityCVars.h"

#include <CryAnimation/CryCharAnimationParams.h>

#include <CryScriptSystem/IScriptSystem.h>

CRYREGISTER_CLASS(CEntityComponentLuaScript);

//////////////////////////////////////////////////////////////////////////
CEntityComponentLuaScript::CEntityComponentLuaScript()
	: m_pScript(nullptr)
	, m_fScriptUpdateRate(0.0f)
	, m_fScriptUpdateTimer(0.0f)
	, m_currentStateId(0)
	, m_implementedUpdateFunction(false)
	, m_isUpdateEnabled(false)
	, m_enableSoundAreaEvents(false)
{
	m_componentFlags.Add(EEntityComponentFlags::Legacy);
	m_componentFlags.Add(EEntityComponentFlags::NoSave);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::ChangeScript(IEntityScript* pScript, SEntitySpawnParams* params)
{
	if (pScript)
	{
		// Release current
		m_pThis.reset();

		m_pScript = static_cast<CEntityScript*>(pScript);
		m_currentStateId = 0;
		m_fScriptUpdateRate = 0;
		m_fScriptUpdateTimer = 0;
		m_enableSoundAreaEvents = false;

		// New object must be created here.
		CreateScriptTable(params);

		m_implementedUpdateFunction = CurrentState()->IsStateFunctionImplemented(ScriptState_OnUpdate);
		m_pEntity->UpdateComponentEventMask(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::EnableScriptUpdate(bool enable)
{
	m_isUpdateEnabled = enable;
	m_pEntity->UpdateComponentEventMask(this);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::CreateScriptTable(SEntitySpawnParams* pSpawnParams)
{
	m_pThis = m_pScript->GetScriptSystem()->CreateTable();
	//m_pThis->Clone( m_pScript->GetScriptTable() );

	if (g_pIEntitySystem)
	{
		//pEntitySystem->GetScriptBindEntity()->DelegateCalls( m_pThis );
		m_pThis->Delegate(m_pScript->GetScriptTable());
	}

	// Clone Properties table recursively.
	if (m_pScript->GetPropertiesTable())
	{
		// If entity have an archetype use shared property table.
		IEntityArchetype* pArchetype = m_pEntity->GetArchetype();
		if (!pArchetype)
		{
			// Custom properties table passed
			if (pSpawnParams && pSpawnParams->pPropertiesTable)
			{
				m_pThis->SetValue(SCRIPT_PROPERTIES_TABLE, pSpawnParams->pPropertiesTable);
			}
			else
			{
				SmartScriptTable pProps;
				pProps.Create(m_pScript->GetScriptSystem());
				pProps->Clone(m_pScript->GetPropertiesTable(), true, true);
				m_pThis->SetValue(SCRIPT_PROPERTIES_TABLE, pProps);
			}
		}
		else
		{
			IScriptTable* pPropertiesTable = pArchetype->GetProperties();
			if (pPropertiesTable)
				m_pThis->SetValue(SCRIPT_PROPERTIES_TABLE, pPropertiesTable);
		}

		SmartScriptTable pEntityPropertiesInstance;
		SmartScriptTable pPropsInst;
		if (m_pThis->GetValue("PropertiesInstance", pEntityPropertiesInstance))
		{
			pPropsInst.Create(m_pScript->GetScriptSystem());
			pPropsInst->Clone(pEntityPropertiesInstance, true, true);
			m_pThis->SetValue("PropertiesInstance", pPropsInst);
		}
	}

	// Set self.__this to this pointer of CScriptProxy
	ScriptHandle handle;
	handle.ptr = m_pEntity;
	m_pThis->SetValue("__this", handle);
	handle.n = m_pEntity->GetId();
	m_pThis->SetValue("id", handle);
	m_pThis->SetValue("class", m_pEntity->GetClass()->GetName());
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::Update(SEntityUpdateContext& ctx)
{
	// Shouldn't be the case, but we must not call Lua with a 0 frametime to avoid potential FPE
	assert(ctx.fFrameTime > FLT_EPSILON);

	if (CVar::pUpdateScript->GetIVal())
	{
		m_fScriptUpdateTimer -= ctx.fFrameTime;
		if (m_fScriptUpdateTimer <= 0)
		{
			ENTITY_PROFILER
			  m_fScriptUpdateTimer = m_fScriptUpdateRate;

			//////////////////////////////////////////////////////////////////////////
			// Script Update.
			m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnUpdate, ctx.fFrameTime);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_UPDATE:
		{
			SEntityUpdateContext* pCtx = (SEntityUpdateContext*)event.nParam[0];
			Update(*pCtx);
		}
		break;
	case ENTITY_EVENT_ANIM_EVENT:
		{
			const AnimEventInstance* const pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
			if (pAnimEvent)
			{
				SmartScriptTable eventData(gEnv->pScriptSystem);
				eventData->SetValue("customParam", pAnimEvent->m_CustomParameter);
				eventData->SetValue("jointName", pAnimEvent->m_BonePathName);
				eventData->SetValue("offset", pAnimEvent->m_vOffset);
				eventData->SetValue("direction", pAnimEvent->m_vDir);
				m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnAnimationEvent, pAnimEvent->m_EventName, eventData);
			}
		}
		break;
	case ENTITY_EVENT_DONE:
		if (m_pScript)
			m_pScript->Call_OnShutDown(m_pThis);
		break;
	case ENTITY_EVENT_RESET:
		// OnReset()
		m_pScript->Call_OnReset(m_pThis.get(), event.nParam[0] == 1);
		break;
	case ENTITY_EVENT_INIT:
		{
			// Call Init.
			CallInitEvent(false);
		}
		break;
	case ENTITY_EVENT_TIMER:
		// OnTimer( nTimerId,nMilliseconds )
		m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnTimer, (int)event.nParam[0], (int)event.nParam[1]);
		break;
	case ENTITY_EVENT_XFORM:
		// OnMove()
		m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnMove);
		break;
	case ENTITY_EVENT_ATTACH:
		// OnBind( childEntity );
		{
			CEntity* pChildEntity = g_pIEntitySystem->GetEntityFromID((EntityId)event.nParam[0]);
			if (pChildEntity)
			{
				IScriptTable* pChildEntityThis = pChildEntity->GetScriptTable();
				if (pChildEntityThis)
					m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnBind, pChildEntityThis);
			}
		}
		break;
	case ENTITY_EVENT_ATTACH_THIS:
		// OnBindThis( ParentEntity );
		{
			CEntity* pParentEntity = g_pIEntitySystem->GetEntityFromID((EntityId)event.nParam[0]);
			if (pParentEntity)
			{
				IScriptTable* pParentEntityThis = pParentEntity->GetScriptTable();
				if (pParentEntityThis)
					m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnBindThis, pParentEntityThis);
				else
					m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnBindThis);
			}
		}
		break;
	case ENTITY_EVENT_DETACH:
		// OnUnbind( childEntity );
		{
			CEntity* pChildEntity = g_pIEntitySystem->GetEntityFromID((EntityId)event.nParam[0]);
			if (pChildEntity)
			{
				IScriptTable* pChildEntityThis = pChildEntity->GetScriptTable();
				if (pChildEntityThis)
					m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnUnBind, pChildEntityThis);
			}
		}
		break;
	case ENTITY_EVENT_DETACH_THIS:
		// OnUnbindThis( ParentEntity );
		{
			CEntity* pParentEntity = g_pIEntitySystem->GetEntityFromID((EntityId)event.nParam[0]);
			if (pParentEntity)
			{
				IScriptTable* pParentEntityThis = pParentEntity->GetScriptTable();
				if (pParentEntityThis)
					m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnUnBindThis, pParentEntityThis);
				else
					m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnUnBindThis);
			}
		}
		break;
	case ENTITY_EVENT_ENTERAREA:
		{
			if (m_enableSoundAreaEvents)
			{
				CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(event.nParam[0]));

				if (pIEntity != nullptr)
				{
					CEntity const* const pTriggerEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(event.nParam[2]));
					IScriptTable* const pTriggerEntityScript = (pTriggerEntity ? pTriggerEntity->GetScriptTable() : nullptr);

					IScriptTable* const pTrgEntityThis = pIEntity->GetScriptTable();

					if (pTrgEntityThis != nullptr)
					{
						if (pTriggerEntityScript != nullptr)
						{
							m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnEnterArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0], pTriggerEntityScript);
						}
						else
						{
							m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnEnterArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
						}
					}

					// allow local player/camera source entities to trigger callback even without that entity having a script table
					if ((pIEntity->GetFlags() & (ENTITY_FLAG_LOCAL_PLAYER | ENTITY_FLAG_CAMERA_SOURCE)) > 0)
					{
						if (pTriggerEntityScript != nullptr)
						{
							m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLocalClientEnterArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0], pTriggerEntityScript);
						}
						else
						{
							m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLocalClientEnterArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
						}
					}

					if ((pIEntity->GetFlagsExtended() & (ENTITY_FLAG_EXTENDED_AUDIO_LISTENER)) > 0)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnAudioListenerEnterArea, pTrgEntityThis);
					}
				}
			}

			break;
		}
	case ENTITY_EVENT_MOVEINSIDEAREA:
		{
			if (m_enableSoundAreaEvents)
			{
				CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(event.nParam[0]));

				if (pIEntity != nullptr)
				{
					IScriptTable* const pTrgEntityThis = pIEntity->GetScriptTable();

					if (pTrgEntityThis != nullptr)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnProceedFadeArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
					}

					// allow local player/camera source entities to trigger callback even without that entity having a scripttable
					if ((pIEntity->GetFlags() & (ENTITY_FLAG_LOCAL_PLAYER | ENTITY_FLAG_CAMERA_SOURCE)) > 0)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLocalClientProceedFadeArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
					}

					if ((pIEntity->GetFlagsExtended() & (ENTITY_FLAG_EXTENDED_AUDIO_LISTENER)) > 0)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnAudioListenerProceedFadeArea, pTrgEntityThis, event.fParam[0]);
					}
				}
			}

			break;
		}
	case ENTITY_EVENT_LEAVEAREA:
		{
			if (m_enableSoundAreaEvents)
			{
				CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(event.nParam[0]));

				if (pIEntity != nullptr)
				{
					CEntity const* const pTriggerEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(event.nParam[2]));
					IScriptTable* const pTriggerEntityScript = (pTriggerEntity ? pTriggerEntity->GetScriptTable() : nullptr);

					IScriptTable* const pTrgEntityThis = pIEntity->GetScriptTable();

					if (pTrgEntityThis != nullptr)
					{
						if (pTriggerEntityScript != nullptr)
						{
							m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLeaveArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0], pTriggerEntityScript);
						}
						else
						{
							m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLeaveArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
						}
					}

					// allow local player/camera source entities to trigger callback even without that entity having a scripttable
					if ((pIEntity->GetFlags() & (ENTITY_FLAG_LOCAL_PLAYER | ENTITY_FLAG_CAMERA_SOURCE)) > 0)
					{
						if (pTriggerEntityScript != nullptr)
						{
							m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLocalClientLeaveArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0], pTriggerEntityScript);
						}
						else
						{
							m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLocalClientLeaveArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
						}
					}

					if ((pIEntity->GetFlagsExtended() & (ENTITY_FLAG_EXTENDED_AUDIO_LISTENER)) > 0)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnAudioListenerLeaveArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0], event.fParam[1]);
					}
				}
			}

			break;
		}
	case ENTITY_EVENT_ENTERNEARAREA:
		{
			if (m_enableSoundAreaEvents)
			{
				CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(event.nParam[0]));

				if (pIEntity != nullptr)
				{
					IScriptTable* const pTrgEntityThis = pIEntity->GetScriptTable();

					if (pTrgEntityThis != nullptr)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnEnterNearArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
					}

					// allow local player/camera source entities to trigger callback even without that entity having a scripttable
					if ((pIEntity->GetFlags() & (ENTITY_FLAG_LOCAL_PLAYER | ENTITY_FLAG_CAMERA_SOURCE)) > 0)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLocalClientEnterNearArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
					}

					if ((pIEntity->GetFlagsExtended() & (ENTITY_FLAG_EXTENDED_AUDIO_LISTENER)) > 0)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnAudioListenerEnterNearArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
					}
				}
			}

			break;
		}
	case ENTITY_EVENT_LEAVENEARAREA:
		{
			if (m_enableSoundAreaEvents)
			{
				CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(event.nParam[0]));

				if (pIEntity != nullptr)
				{
					IScriptTable* const pTrgEntityThis = pIEntity->GetScriptTable();

					if (pTrgEntityThis != nullptr)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLeaveNearArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
					}

					// allow local player/camera source entities to trigger callback even without that entity having a scripttable
					if ((pIEntity->GetFlags() & (ENTITY_FLAG_LOCAL_PLAYER | ENTITY_FLAG_CAMERA_SOURCE)) > 0)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLocalClientLeaveNearArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
					}

					if ((pIEntity->GetFlagsExtended() & (ENTITY_FLAG_EXTENDED_AUDIO_LISTENER)) > 0)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnAudioListenerLeaveNearArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0], event.fParam[1]);
					}
				}
			}

			break;
		}
	case ENTITY_EVENT_MOVENEARAREA:
		{
			if (m_enableSoundAreaEvents)
			{
				CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(event.nParam[0]));

				if (pIEntity != nullptr)
				{
					IScriptTable* const pTrgEntityThis = pIEntity->GetScriptTable();

					if (pTrgEntityThis != nullptr)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnMoveNearArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0], event.fParam[1]);
					}

					// allow local player/camera source entities to trigger callback even without that entity having a scripttable
					if ((pIEntity->GetFlags() & (ENTITY_FLAG_LOCAL_PLAYER | ENTITY_FLAG_CAMERA_SOURCE)) > 0)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLocalClientMoveNearArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0], event.fParam[1]);
					}

					if ((pIEntity->GetFlagsExtended() & (ENTITY_FLAG_EXTENDED_AUDIO_LISTENER)) > 0)
					{
						m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnAudioListenerMoveNearArea, pTrgEntityThis, static_cast<int>(event.nParam[1]), event.fParam[0]);
					}
				}
			}

			break;
		}
	case ENTITY_EVENT_PHYS_BREAK:
		{
			EventPhysJointBroken* pBreakEvent = (EventPhysJointBroken*)event.nParam[0];
			if (pBreakEvent)
			{
				Vec3 pBreakPos = pBreakEvent->pt;
				int nBreakPartId = pBreakEvent->partid[0];
				int nBreakOtherEntityPartId = pBreakEvent->partid[1];
				m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnPhysicsBreak, pBreakPos, nBreakPartId, nBreakOtherEntityPartId);
			}
		}
		break;
	case ENTITY_EVENT_AUDIO_TRIGGER_ENDED:
		{
			CryAudio::SRequestInfo const* const pRequestInfo = reinterpret_cast<CryAudio::SRequestInfo const* const>(event.nParam[0]);
			ScriptHandle handle;
			handle.n = static_cast<UINT_PTR>(pRequestInfo->audioControlId);
			m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnSoundDone, handle);
		}
		break;
	case ENTITY_EVENT_LEVEL_LOADED:
		m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnLevelLoaded);
		break;
	case ENTITY_EVENT_START_LEVEL:
		m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnStartLevel);
		break;
	case ENTITY_EVENT_START_GAME:
		m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnStartGame);
		break;

	case ENTITY_EVENT_PRE_SERIALIZE:
		// Kill all timers.
		{
			// If state changed kill all old timers.
			m_pEntity->KillTimer(IEntity::KILL_ALL_TIMER);
			m_currentStateId = 0;
		}
		break;

	case ENTITY_EVENT_POST_SERIALIZE:
		if (m_pThis->HaveValue("OnPostLoad"))
		{
			Script::CallMethod(m_pThis.get(), "OnPostLoad");
		}
		break;
	case ENTITY_EVENT_HIDE:
		{
			m_pEntity->UpdateComponentEventMask(this);

			m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnHidden);
		}
		break;
	case ENTITY_EVENT_UNHIDE:
		{
			m_pEntity->UpdateComponentEventMask(this);

			m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnUnhidden);
		}
		break;
	case ENTITY_EVENT_XFORM_FINISHED_EDITOR:
		m_pScript->Call_OnTransformFromEditorDone(m_pThis);
		break;
	case ENTITY_EVENT_COLLISION:
		{
			const EventPhysCollision* pCollision = reinterpret_cast<const EventPhysCollision*>(event.nParam[0]);

			const int thisEntityIndex = static_cast<int>(event.nParam[1]);
			const int otherEntityIndex = (thisEntityIndex + 1) % 2;

			CEntity* pOtherEntity = static_cast<CEntity*>(pCollision->pEntity[otherEntityIndex]->GetForeignData(PHYS_FOREIGN_ID_ENTITY));

			const Vec3& normal = thisEntityIndex == 0 ? pCollision->n : -pCollision->n;
			OnCollision(pOtherEntity, pCollision->idmat[otherEntityIndex], pCollision->pt, normal, pCollision->vloc[thisEntityIndex], pCollision->vloc[otherEntityIndex], pCollision->partid[otherEntityIndex], pCollision->mass[otherEntityIndex]);
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
uint64 CEntityComponentLuaScript::GetEventMask() const
{
	uint64 eventMask = ENTITY_EVENT_BIT(ENTITY_EVENT_ANIM_EVENT) | ENTITY_EVENT_BIT(ENTITY_EVENT_DONE) | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_INIT)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_TIMER) | ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM) | ENTITY_EVENT_BIT(ENTITY_EVENT_ATTACH) | ENTITY_EVENT_BIT(ENTITY_EVENT_ATTACH_THIS)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_DETACH) | ENTITY_EVENT_BIT(ENTITY_EVENT_DETACH_THIS) | ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_MOVEINSIDEAREA)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVEAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERNEARAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVENEARAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_MOVENEARAREA)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_PHYS_BREAK) | ENTITY_EVENT_BIT(ENTITY_EVENT_AUDIO_TRIGGER_ENDED) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEVEL_LOADED) | ENTITY_EVENT_BIT(ENTITY_EVENT_START_LEVEL)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME) | ENTITY_EVENT_BIT(ENTITY_EVENT_PRE_SERIALIZE) | ENTITY_EVENT_BIT(ENTITY_EVENT_POST_SERIALIZE) | ENTITY_EVENT_BIT(ENTITY_EVENT_HIDE)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_UNHIDE) | ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM_FINISHED_EDITOR) | ENTITY_EVENT_BIT(ENTITY_EVENT_COLLISION);

	if (m_implementedUpdateFunction && m_isUpdateEnabled && (!m_pEntity->IsHidden() || (m_pEntity->GetFlags() & ENTITY_FLAG_UPDATE_HIDDEN) != 0))
	{
		eventMask |= ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE);
	}

	return eventMask;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityComponentLuaScript::GotoState(const char* sStateName)
{
	int nStateId = m_pScript->GetStateId(sStateName);
	if (nStateId >= 0)
	{
		GotoState(nStateId);
	}
	else
	{
		EntityWarning("GotoState called with unknown state %s, in entity %s", sStateName, m_pEntity->GetEntityTextDescription().c_str());
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityComponentLuaScript::GotoState(int stateId)
{
	if (stateId == m_currentStateId)
		return true; // Already in this state.

	// Call End state event.
	m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnEndState);

	// If state changed kill all old timers.
	m_pEntity->KillTimer(IEntity::KILL_ALL_TIMER);

	SEntityEvent levent;
	levent.event = ENTITY_EVENT_LEAVE_SCRIPT_STATE;
	levent.nParam[0] = m_currentStateId;
	levent.nParam[1] = 0;
	m_pEntity->SendEvent(levent);

	m_currentStateId = stateId;

	// Call BeginState event.
	m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnBeginState);

	//////////////////////////////////////////////////////////////////////////
	// Repeat check if update script function is implemented.
	m_implementedUpdateFunction = CurrentState()->IsStateFunctionImplemented(ScriptState_OnUpdate);
	m_pEntity->UpdateComponentEventMask(this);

	/*
	   //////////////////////////////////////////////////////////////////////////
	   // Check if need ResolveCollisions for OnContact script function.
	   m_bUpdateOnContact = IsStateFunctionImplemented(ScriptState_OnContact);
	   //////////////////////////////////////////////////////////////////////////
	 */

	if (gEnv->bMultiplayer && (m_pEntity->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY)) == 0)
	{
		gEnv->pNetContext->ChangedAspects(m_pEntity->GetId(), eEA_Script);
	}

	SEntityEvent eevent;
	eevent.event = ENTITY_EVENT_ENTER_SCRIPT_STATE;
	eevent.nParam[0] = m_currentStateId;
	eevent.nParam[1] = 0;
	m_pEntity->SendEvent(eevent);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityComponentLuaScript::IsInState(const char* sStateName)
{
	int nStateId = m_pScript->GetStateId(sStateName);
	if (nStateId >= 0)
	{
		return nStateId == m_currentStateId;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityComponentLuaScript::IsInState(int nState)
{
	return nState == m_currentStateId;
}

//////////////////////////////////////////////////////////////////////////
const char* CEntityComponentLuaScript::GetState()
{
	return m_pScript->GetStateName(m_currentStateId);
}

//////////////////////////////////////////////////////////////////////////
int CEntityComponentLuaScript::GetStateId()
{
	return m_currentStateId;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityComponentLuaScript::NeedGameSerialize()
{
	if (!m_pThis)
		return false;

	if (m_fScriptUpdateRate != 0 || m_currentStateId != 0)
		return true;

	if (CVar::pEnableFullScriptSave && CVar::pEnableFullScriptSave->GetIVal())
		return true;

	if (m_pThis->HaveValue("OnSave") && m_pThis->HaveValue("OnLoad"))
		return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::SerializeProperties(TSerialize ser)
{
	if (ser.GetSerializationTarget() == eST_Network)
		return;

	// Saving.
	if (!(m_pEntity->GetFlags() & ENTITY_FLAG_UNREMOVABLE))
	{

		// Properties never serialized
		/*
		   if (!m_pEntity->GetArchetype())
		   {
		   // If not archetype also serialize properties table of the entity.
		   SerializeTable( ser, "Properties" );
		   }*/

		// Instance table always initialized for dynamic entities.
		SerializeTable(ser, "PropertiesInstance");
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::GameSerialize(TSerialize ser)
{
	CHECK_SCRIPT_STACK;

	if (ser.GetSerializationTarget() != eST_Network)
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Script proxy serialization");

		if (NeedGameSerialize())
		{
			if (ser.BeginOptionalGroup("ScriptProxy", true))
			{
				ser.Value("scriptUpdateRate", m_fScriptUpdateRate);
				int currStateId = m_currentStateId;
				ser.Value("currStateId", currStateId);

				// Simulate state change
				if (m_currentStateId != currStateId)
				{
					// If state changed kill all old timers.
					m_pEntity->KillTimer(IEntity::KILL_ALL_TIMER);
					m_currentStateId = currStateId;
				}
				if (ser.IsReading())
				{
					// Repeat check if update script function is implemented.
					m_implementedUpdateFunction = CurrentState()->IsStateFunctionImplemented(ScriptState_OnUpdate);
					m_pEntity->UpdateComponentEventMask(this);
				}

				if (CVar::pEnableFullScriptSave && CVar::pEnableFullScriptSave->GetIVal())
				{
					ser.Value("FullScriptData", m_pThis.get());
				}
				else if (m_pThis->HaveValue("OnSave") && m_pThis->HaveValue("OnLoad"))
				{
					//SerializeTable( ser, "Properties" );
					//SerializeTable( ser, "PropertiesInstance" );
					//SerializeTable( ser, "Events" );

					SmartScriptTable persistTable(m_pThis->GetScriptSystem());
					if (ser.IsWriting())
						Script::CallMethod(m_pThis.get(), "OnSave", persistTable);
					ser.Value("ScriptData", persistTable.GetPtr());
					if (ser.IsReading())
						Script::CallMethod(m_pThis.get(), "OnLoad", persistTable);
				}

				ser.EndGroup(); //ScriptProxy
			}
			else
			{
				// If we haven't already serialized the script proxy then reset it back to the default state.

				CRY_ASSERT(m_pScript);

				m_pScript->Call_OnReset(m_pThis.get(), true);
			}
		}
	}
	else
	{
		int stateId = m_currentStateId;
		ser.Value("currStateId", stateId, 'sSts');
		if (ser.IsReading())
			GotoState(stateId);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::SerializeTable(TSerialize ser, const char* name)
{
	CHECK_SCRIPT_STACK;

	SmartScriptTable table;
	if (ser.IsReading())
	{
		if (ser.BeginOptionalGroup(name, true))
		{
			table = SmartScriptTable(m_pThis->GetScriptSystem());
			ser.Value("table", table);
			m_pThis->SetValue(name, table);
			ser.EndGroup();
		}
	}
	else
	{
		if (m_pThis->GetValue(name, table))
		{
			ser.BeginGroup(name);
			ser.Value("table", table);
			ser.EndGroup();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::LegacySerializeXML(XmlNodeRef& entityNode, XmlNodeRef& componentNode, bool bLoading)
{
	// Initialize script properties.
	if (bLoading)
	{
		CScriptProperties scriptProps;
		// Initialize properties.
		// Script properties are currently stored in the entity node, not the component itself (legacy)
		scriptProps.SetProperties(entityNode, m_pThis);

		XmlNodeRef eventTargets = entityNode->findChild("EventTargets");
		if (eventTargets)
			SetEventTargets(eventTargets);
	}
}

//////////////////////////////////////////////////////////////////////////
struct SEntityEventTarget
{
	string   event;
	EntityId target;
	string   sourceEvent;
};

//////////////////////////////////////////////////////////////////////////
// Set event targets from the XmlNode exported by Editor.
void CEntityComponentLuaScript::SetEventTargets(XmlNodeRef& eventTargetsNode)
{
	std::set<string> sourceEvents;
	std::vector<SEntityEventTarget> eventTargets;

	IScriptSystem* pSS = GetIScriptSystem();

	for (int i = 0; i < eventTargetsNode->getChildCount(); i++)
	{
		XmlNodeRef eventNode = eventTargetsNode->getChild(i);

		SEntityEventTarget et;
		et.event = eventNode->getAttr("Event");
		if (!eventNode->getAttr("Target", et.target))
			et.target = 0; // failed reading...
		et.sourceEvent = eventNode->getAttr("SourceEvent");

		if (et.event.empty() || !et.target || et.sourceEvent.empty())
			continue;

		eventTargets.push_back(et);
		sourceEvents.insert(et.sourceEvent);
	}
	if (eventTargets.empty())
		return;

	SmartScriptTable pEventsTable;

	if (!m_pThis->GetValue("Events", pEventsTable))
	{
		pEventsTable = pSS->CreateTable();
		// assign events table to the entity self script table.
		m_pThis->SetValue("Events", pEventsTable);
	}

	for (auto const& sourceEvent : sourceEvents)
	{
		SmartScriptTable pTrgEvents(pSS);

		pEventsTable->SetValue(sourceEvent.c_str(), pTrgEvents);

		// Put target events to table.
		int trgEventIndex = 1;

		for (auto const& et : eventTargets)
		{
			if (et.sourceEvent == sourceEvent)
			{
				SmartScriptTable pTrgEvent(pSS);

				pTrgEvents->SetAt(trgEventIndex, pTrgEvent);
				trgEventIndex++;
				ScriptHandle hdl;
				hdl.n = et.target;
				pTrgEvent->SetAt(1, hdl);
				pTrgEvent->SetAt(2, et.event.c_str());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::CallEvent(const char* sEvent)
{
	m_pScript->CallEvent(m_pThis.get(), sEvent, (bool)true);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::CallEvent(const char* sEvent, float fValue)
{
	m_pScript->CallEvent(m_pThis.get(), sEvent, fValue);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::CallEvent(const char* sEvent, bool bValue)
{
	m_pScript->CallEvent(m_pThis.get(), sEvent, bValue);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::CallEvent(const char* sEvent, const char* sValue)
{
	m_pScript->CallEvent(m_pThis.get(), sEvent, sValue);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::CallEvent(const char* sEvent, EntityId nEntityId)
{
	IScriptTable* pTable = nullptr;
	CEntity* const pIEntity = g_pIEntitySystem->GetEntityFromID(nEntityId);

	if (pIEntity != nullptr)
	{
		pTable = pIEntity->GetScriptTable();
	}

	m_pScript->CallEvent(m_pThis.get(), sEvent, pTable);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::CallEvent(const char* sEvent, const Vec3& vValue)
{
	m_pScript->CallEvent(m_pThis.get(), sEvent, vValue);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::CallInitEvent(bool bFromReload)
{
	m_pScript->Call_OnInit(m_pThis.get(), bFromReload);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::OnCollision(CEntity* pTarget, int matId, const Vec3& pt, const Vec3& n, const Vec3& vel, const Vec3& targetVel, int partId, float mass)
{
	if (CVar::pLogCollisions->GetIVal() != 0)
	{
		string s1 = GetEntity()->GetEntityTextDescription();
		string s2;
		if (pTarget)
			s2 = pTarget->GetEntityTextDescription();
		else
			s2 = "<Unknown>";
		CryLogAlways("OnCollision %s (Target: %s)", s1.c_str(), s2.c_str());
	}

	if (!CurrentState()->IsStateFunctionImplemented(ScriptState_OnCollision))
		return;

	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if (!m_hitTable)
		m_hitTable.Create(gEnv->pScriptSystem);
	{
		Vec3 dir(0, 0, 0);
		CScriptSetGetChain chain(m_hitTable);
		chain.SetValue("normal", n);
		chain.SetValue("pos", pt);

		if (vel.GetLengthSquared() > 1e-6f)
		{
			dir = vel.GetNormalized();
			chain.SetValue("dir", dir);
		}

		chain.SetValue("velocity", vel);
		chain.SetValue("target_velocity", targetVel);
		chain.SetValue("target_mass", mass);
		chain.SetValue("partid", partId);
		chain.SetValue("backface", n.Dot(dir) >= 0);
		chain.SetValue("materialId", matId);

		if (pTarget)
		{
			ScriptHandle sh;
			sh.n = pTarget->GetId();

			if (pTarget->GetPhysics())
			{
				chain.SetValue("target_type", (int)pTarget->GetPhysics()->GetType());
			}
			else
			{
				chain.SetToNull("target_type");
			}

			chain.SetValue("target_id", sh);

			if (pTarget->GetScriptTable())
			{
				chain.SetValue("target", pTarget->GetScriptTable());
			}
		}
		else
		{
			chain.SetToNull("target_type");
			chain.SetToNull("target_id");
			chain.SetToNull("target");
		}
	}

	m_pScript->CallStateFunction(CurrentState(), m_pThis.get(), ScriptState_OnCollision, m_hitTable);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::SendScriptEvent(int Event, IScriptTable* pParamters, bool* pRet)
{
	SScriptState* pState = CurrentState();
	for (int i = 0; i < NUM_STATES; i++)
	{
		if (m_pScript->ShouldExecuteCall(i) && pState->pStateFuns[i] && pState->pStateFuns[i]->pFunction[ScriptState_OnEvent])
		{
			if (pRet)
				Script::CallReturn(GetIScriptSystem(), pState->pStateFuns[i]->pFunction[ScriptState_OnEvent], m_pThis.get(), Event, pParamters, *pRet);
			else
				Script::Call(GetIScriptSystem(), pState->pStateFuns[i]->pFunction[ScriptState_OnEvent], m_pThis.get(), Event, pParamters);
			pRet = nullptr;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::SendScriptEvent(int Event, const char* str, bool* pRet)
{
	SScriptState* pState = CurrentState();
	for (int i = 0; i < NUM_STATES; i++)
	{
		if (m_pScript->ShouldExecuteCall(i) && pState->pStateFuns[i] && pState->pStateFuns[i]->pFunction[ScriptState_OnEvent])
		{
			if (pRet)
				Script::CallReturn(GetIScriptSystem(), pState->pStateFuns[i]->pFunction[ScriptState_OnEvent], m_pThis.get(), Event, str, *pRet);
			else
				Script::Call(GetIScriptSystem(), pState->pStateFuns[i]->pFunction[ScriptState_OnEvent], m_pThis.get(), Event, str);
			pRet = nullptr;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentLuaScript::SendScriptEvent(int Event, int nParam, bool* pRet)
{
	SScriptState* pState = CurrentState();
	for (int i = 0; i < NUM_STATES; i++)
	{
		if (m_pScript->ShouldExecuteCall(i) && pState->pStateFuns[i] && pState->pStateFuns[i]->pFunction[ScriptState_OnEvent])
		{
			if (pRet)
				Script::CallReturn(GetIScriptSystem(), pState->pStateFuns[i]->pFunction[ScriptState_OnEvent], m_pThis.get(), Event, nParam, *pRet);
			else
				Script::Call(GetIScriptSystem(), pState->pStateFuns[i]->pFunction[ScriptState_OnEvent], m_pThis.get(), Event, nParam);
			pRet = nullptr;
		}
	}
}

void CEntityComponentLuaScript::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}

void CEntityComponentLuaScript::SetPhysParams(int type, IScriptTable* params)
{
	// This function can currently be called by a component created without an entity, hence the special case
	IPhysicalEntity* pPhysicalEntity = m_pEntity != nullptr ? m_pEntity->GetPhysics() : nullptr;
	g_pIEntitySystem->GetScriptBindEntity()->SetEntityPhysicParams(nullptr, pPhysicalEntity, type, params);
}
