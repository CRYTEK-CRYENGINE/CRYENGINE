// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   Entity.h
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Entity.h"
#include "AffineParts.h"
#include "EntitySystem.h"
#include <CryEntitySystem/IEntityClass.h>
#include "EntityAudioProxy.h"
#include "AreaProxy.h"
#include "FlowGraphProxy.h"
#include <CryNetwork/ISerialize.h>
#include "NetEntity.h"
#include "TriggerProxy.h"
#include "EntityNodeProxy.h"
#include "PartitionGrid.h"
#include "ProximityTriggerSystem.h"
#include "CameraProxy.h"
#include "EntitySlot.h"
#include "RopeProxy.h"
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAIActorProxy.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryAISystem/IAIObjectManager.h>
#include <CryAISystem/IAIActor.h>
#include "EntityLayer.h"
#include "GeomCacheAttachmentManager.h"
#include "CharacterBoneAttachmentManager.h"
#include <CryString/StringUtils.h>
#include "ClipVolumeProxy.h"
#include "DynamicResponseProxy.h"
#include <CryExtension/CryCreateClassInstance.h>
#include <CryGame/IGameFramework.h>
#include <CrySchematyc/CoreAPI.h>
#include <CrySchematyc/Utils/ClassProperties.h>

// enable this to check nan's on position updates... useful for debugging some weird crashes
#define ENABLE_NAN_CHECK

#ifdef ENABLE_NAN_CHECK
	#define CHECKQNAN_FLT(x) \
	  assert(*alias_cast<unsigned*>(&x) != 0xffffffffu)
#else
	#define CHECKQNAN_FLT(x) (void*)0
#endif

#define CHECKQNAN_VEC(v) \
  CHECKQNAN_FLT(v.x); CHECKQNAN_FLT(v.y); CHECKQNAN_FLT(v.z)

//////////////////////////////////////////////////////////////////////////
CEntity::CEntity(SEntitySpawnParams& params)
{
	m_render.m_pEntity = this;
	m_physics.m_pEntity = this;

	m_pClass = params.pClass;
	m_pArchetype = params.pArchetype;
	m_nID = params.id;
	m_flags = params.nFlags;
	m_flagsExtended = params.nFlagsExtended;
	m_guid = params.guid;

	// Set flags.
	m_bRequiresComponentUpdate = 0;
	m_bInActiveList = 0;

	m_bInitialized = 0;
	m_bHidden = 0;
	m_bIsInHiddenLayer = 0;
	m_bInvisible = 0;
	m_bGarbage = 0;
	m_nUpdateCounter = 0;

	m_bTrigger = 0;
	m_bWasRelocated = 0;
	m_bNotInheritXform = 0;
	m_bInShutDown = 0;
	m_bLoadedFromLevelFile = 0;

	m_pGridLocation = 0;
	m_pProximityEntity = 0;

	m_eUpdatePolicy = ENTITY_UPDATE_NEVER;
	m_aiObjectID = INVALID_AIOBJECTID;

	m_pEntityLinks = 0;

	// Forward dir cache is initially invalid
	m_bDirtyForwardDir = true;

	m_objectID = 0;

#ifdef SEG_WORLD
	m_bLocalSeg = false;
#endif

	//////////////////////////////////////////////////////////////////////////
	// Initialize basic parameters.
	//////////////////////////////////////////////////////////////////////////
	if (params.sName && params.sName[0] != '\0')
		SetName(params.sName);
	CHECKQNAN_VEC(params.vPosition);
	m_vPos = params.vPosition;
	m_qRotation = params.qRotation;
	m_vScale = params.vScale;

	CalcLocalTM(m_worldTM);

	ComputeForwardDir();

	//////////////////////////////////////////////////////////////////////////
	// Check if entity needs to create a script proxy.
	IEntityScript* pEntityScript = m_pClass->GetIEntityScript();
	if (pEntityScript)
	{
		// Creates a script proxy.
		CEntityScript* pCEntityScript = static_cast<CEntityScript*>(pEntityScript);
		if (pCEntityScript->LoadScript())
		{
			auto pScriptProxy = GetOrCreateComponent<IEntityScriptComponent>();
			pScriptProxy->ChangeScript(pEntityScript, &params);
		}
	}

	m_nKeepAliveCounter = 0;

	m_cloneLayerId = -1;

	// #netentity Will be addressed in BindToNetwork-refactoring
	m_pNetEntity = std::unique_ptr<INetEntity>(new CNetEntity(this));
}

//////////////////////////////////////////////////////////////////////////
CEntity::~CEntity()
{
	assert(m_nKeepAliveCounter == 0);

	// Components could still be referring to m_szName, so clear them before it gets destroyed
	m_components.Clear();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetName(const char* sName)
{
	if ((m_flags & ENTITY_FLAG_UNREMOVABLE) && m_bInitialized && !gEnv->IsEditor())
		CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_ERROR, "!Unremovable entities should never change their name! This is non-compatible with C2 savegames.");

	g_pIEntitySystem->ChangeEntityName(this, sName);
	if (IAIObject* pAIObject = GetAIObject())
		pAIObject->SetName(sName);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetFlags(uint32 flags)
{
	if (flags != m_flags)
	{
		m_flags = flags;
		m_render.UpdateRenderNodes();
		if (m_pGridLocation)
			m_pGridLocation->nEntityFlags = flags;
	}
};

//////////////////////////////////////////////////////////////////////////
void CEntity::SetFlagsExtended(uint32 flagsExtended)
{
	if (flagsExtended != m_flagsExtended)
	{
		m_flagsExtended = flagsExtended;
		m_render.UpdateRenderNodes();
	}
};

//////////////////////////////////////////////////////////////////////////
bool CEntity::SendEvent(SEntityEvent& event)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	CTimeValue t0;
	if (CVar::es_DebugEvents)
	{
		t0 = gEnv->pTimer->GetAsyncTime();
	}

	if (CVar::es_DisableTriggers)
	{
		static IEntityClass* pProximityTriggerClass = NULL;
		static IEntityClass* pAreaTriggerClass = NULL;

		if (pProximityTriggerClass == NULL || pAreaTriggerClass == NULL)
		{
			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

			if (pEntitySystem != NULL)
			{
				IEntityClassRegistry* pClassRegistry = pEntitySystem->GetClassRegistry();

				if (pClassRegistry != NULL)
				{
					pProximityTriggerClass = pClassRegistry->FindClass("ProximityTrigger");
					pAreaTriggerClass = pClassRegistry->FindClass("AreaTrigger");
				}
			}
		}

		IEntityClass* pEntityClass = GetClass();
		if (pEntityClass == pProximityTriggerClass || pEntityClass == pAreaTriggerClass)
		{
			if (event.event == ENTITY_EVENT_ENTERAREA || event.event == ENTITY_EVENT_LEAVEAREA)
			{
				return true;
			}
		}
	}

	// AI object position must be updated first so proxies could eventually use it (as CSmartObjects does)
	switch (event.event)
	{
	case ENTITY_EVENT_XFORM:
		if (!m_bGarbage)
		{
			UpdateAIObject();
		}
		break;
	case ENTITY_EVENT_ENTERAREA:
	case ENTITY_EVENT_HIDE:
	case ENTITY_EVENT_UNHIDE:
		{
			if (!m_bGarbage)
			{
				// Notify audio proxies before script proxies!
				auto pIEntityAudioComponent = GetComponent<IEntityAudioComponent>();

				if (pIEntityAudioComponent != NULL)
				{
					pIEntityAudioComponent->ProcessEvent(event);
				}
			}

			break;
		}
	case ENTITY_EVENT_RESET:
		{
			if (m_simulationMode != EEntitySimulationMode::Preview)
			{
				m_simulationMode = event.nParam[0] == 1 ? EEntitySimulationMode::Game : EEntitySimulationMode::Editor;
			}

			// Activate entity if was deactivated:
			if (m_bGarbage)
			{
				m_bGarbage = false;
				// If entity was deleted in game, ressurect it.
				SEntityEvent entevnt;
				entevnt.event = ENTITY_EVENT_INIT;
				SendEvent(entevnt);
			}

			//CryLogAlways( "%s became visible",GetEntityTextDescription() );
			// [marco] needed when going in and out of game mode in the editor
			{
				m_render.SetLastSeenTime(gEnv->pTimer->GetCurrTime());
				ICharacterInstance* pCharacterInstance = m_render.GetCharacter(0);
				if (pCharacterInstance)
					pCharacterInstance->SetPlaybackScale(1.0f);
			}

			// We only want to reset when we return from game mode to editor mode.
			if (m_pSchematycObject && gEnv->IsEditor() && !gEnv->IsEditorGameMode())
			{
				m_pSchematycObject->SetSimulationMode(m_simulationMode, Schematyc::EObjectSimulationUpdatePolicy::OnChangeOnly, false);
			}
			break;
		}
	case ENTITY_EVENT_INIT:
		m_bGarbage = false;
		break;

	case ENTITY_EVENT_ANIM_EVENT:
		{
			// If the event is a sound event, make sure we have a sound proxy.
			const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
			const char* eventName = (pAnimEvent ? pAnimEvent->m_EventName : 0);
			if (eventName && stricmp(eventName, "sound") == 0)
			{
				GetOrCreateComponent<IEntityAudioComponent>();
			}
		}
		break;

	case ENTITY_EVENT_DONE:
		// When deleting should detach all children.
		{
			//g_pIEntitySystem->RemoveTimerEvent(GetId(), -1);
			DetachAll();
			DetachThis(0);

			IPhysicalEntity* pPhysics = GetPhysics();
			if (pPhysics && pPhysics->GetForeignData(PHYS_FOREIGN_ID_ENTITY))
			{
				pe_params_foreign_data pfd;
				pfd.pForeignData = 0;
				pfd.iForeignData = -1;
				pPhysics->SetParams(&pfd, 1);
			}
		}
		break;

	case ENTITY_EVENT_PRE_SERIALIZE:
		//filter out event if not using save/load
		if (!g_pIEntitySystem->ShouldSerializedEntity(this))
			return true;
		break;

	case ENTITY_EVENT_LEVEL_LOADED:
		{
			// After level load we set the simulation mode but don't start simulation yet. Mean we
			// fully prepare the object for the simulation to start. Simulation will be started with
			// the ENTITY_EVENT_START_GAME event.
			if (m_pSchematycObject)
			{
				m_simulationMode = !gEnv->IsEditing() ? EEntitySimulationMode::Game : EEntitySimulationMode::Editor;
				m_pSchematycObject->SetSimulationMode(m_simulationMode, Schematyc::EObjectSimulationUpdatePolicy::OnChangeOnly, false);
			}
		}
		break;

	case ENTITY_EVENT_START_GAME:
		{
			// Set simulation mode and finally start simulation.
			if (m_pSchematycObject)
			{
				m_pSchematycObject->SetSimulationMode(m_simulationMode, Schematyc::EObjectSimulationUpdatePolicy::OnChangeOnly, true);
			}
		}
		break;
	}

	if (!m_bGarbage)
	{
		// Broadcast event to proxies.
		m_render.ProcessEvent(event);
		m_physics.ProcessEvent(event);

		// Send event to components.
		m_components.ForEachSorted(
		  [&event](const SEntityComponentRecord& componentRecord)
		{
			if (componentRecord.registeredEventsMask & BIT64(event.event))
			{
			  componentRecord.pComponent->ProcessEvent(event);
			}
		}
		  );

		// Give entity system a chance to check the event, and notify other listeners.
		uint32 nWhyFlags = (uint32)event.nParam[0];
		if (event.event != ENTITY_EVENT_XFORM || !(nWhyFlags & ENTITY_XFORM_NO_SEND_TO_ENTITY_SYSTEM))
		{
			if (m_pEventListeners)
			{
				if (m_pEventListeners->haveListenersMask & BIT64(event.event))
				{
					// Fast mask check to see if any listeners for this event type existed on entity
					m_pEventListeners->m_listeners.ForEach(
					  [this, &event](const SEventListeners::SListener& l)
					{
						if (l.event == event.event)
							l.pListener->OnEntityEvent(this, event);
					});
				}
			}
			g_pIEntitySystem->OnEntityEvent(this, event);
		}

#ifndef _RELEASE
		if (CVar::es_DebugEvents)
		{
			CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
			LogEvent(event, t1 - t0);
		}
#endif

		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////
bool CEntity::Init(SEntitySpawnParams& params)
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Entity, 0, "Init: %s", params.sName ? params.sName : "(noname)");

	{
		bool bIsPreview = (params.nFlagsExtended & ENTITY_FLAG_EXTENDED_PREVIEW) != 0;

		if (bIsPreview)
		{
			m_simulationMode = Schematyc::ESimulationMode::Preview;
		}
		else if (gEnv->IsEditing() && !g_pIEntitySystem->IsLoadingLevel())
		{
			m_simulationMode = EEntitySimulationMode::Editor;
		}
		else if (gEnv->pGameFramework->IsGameStarted())
		{
			m_simulationMode = EEntitySimulationMode::Game;
		}
		else
		{
			m_simulationMode = EEntitySimulationMode::Idle;
		}
	}

	CEntityClass* pClass = static_cast<CEntityClass*>(params.pClass);

	// Create Schematyc object is class have runtime class for it.
	if (pClass->GetSchematycRuntimeClass())
	{
		CreateSchematycObject(params);
	}

	const IEntityClass::OnSpawnCallback& onSpawnCallback = params.pClass->GetOnSpawnCallback();
	if (onSpawnCallback)
	{
		if (!onSpawnCallback(*this, params))
		{
			return false;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Check if entity needs to create a proxy class.
	IEntityClass::UserProxyCreateFunc pUserProxyCreateFunc = params.pClass->GetUserProxyCreateFunc();
	if (pUserProxyCreateFunc)
	{
		pUserProxyCreateFunc(this, params, params.pClass->GetUserProxyData());
	}

	//////////////////////////////////////////////////////////////////////////
	// Before calling Init event serialize entity from xml node
	//////////////////////////////////////////////////////////////////////////
	if (params.entityNode)
	{
		SerializeXML(params.entityNode, true, false);
	}

	SEntityEvent entevnt;
	entevnt.event = ENTITY_EVENT_INIT;
	SendEvent(entevnt);

	// Make sure position is registered.
	if (!m_bWasRelocated)
		OnRellocate(ENTITY_XFORM_POS);

	if (params.entityNode)
	{
		// Physics state serialization must be after full initialization
		m_physics.SerializeXML(params.entityNode, true);
	}

	//Render Proxy is initialized last.
	m_render.PostInit();
	m_bInitialized = true;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Update(SEntityUpdateContext& ctx)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	if (m_bHidden && !CheckFlags(ENTITY_FLAG_UPDATE_HIDDEN))
		return;

	// Broadcast event to proxies.
	SEntityEvent event;
	event.event = ENTITY_EVENT_UPDATE;
	event.nParam[0] = (INT_PTR)&ctx;
	event.fParam[0] = ctx.fFrameTime;

	m_components.ForEachSorted(
	  [&event](const SEntityComponentRecord& componentRecord)
	{
		if (componentRecord.registeredEventsMask & BIT64(ENTITY_EVENT_UPDATE))
		{
		  componentRecord.pComponent->ProcessEvent(event);
		}
	}
	  );

	// Validate if any components where updated bounding box of entity could have potentially changed
	m_render.CheckLocalBoundsChanged();

	if (m_nUpdateCounter != 0)
	{
		if (--m_nUpdateCounter == 0)
		{
			ActivateEntityIfNecessary();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::PrePhysicsUpdate(SEntityEvent& event)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	m_components.ForEachSorted(
	  [&event](const SEntityComponentRecord& componentRecord)
	{
		if (componentRecord.registeredEventsMask & BIT64(ENTITY_EVENT_PREPHYSICSUPDATE))
		{
		  componentRecord.pComponent->ProcessEvent(event);
		}
	}
	  );
}

bool CEntity::IsPrePhysicsActive()
{
	return g_pIEntitySystem->IsPrePhysicsActive(this);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::ShutDown()
{
	ENTITY_PROFILER;

	m_bInShutDown = true;

	RemoveAllEntityLinks();
	m_physics.ReleasePhysicalEntity();

	ClearSlots();

	// Remove entity name from the map.
	g_pIEntitySystem->ChangeEntityName(this, "");

	if (m_pGridLocation)
	{
		// Free grid location.
		g_pIEntitySystem->GetPartitionGrid()->FreeLocation(m_pGridLocation);
		m_pGridLocation = 0;
		m_flags |= ENTITY_FLAG_NO_PROXIMITY;
	}

	if (m_pProximityEntity)
	{
		g_pIEntitySystem->GetProximityTriggerSystem()->RemoveEntity(m_pProximityEntity, true);
		m_pProximityEntity = 0;
	}

	if (m_flags & ENTITY_FLAG_TRIGGER_AREAS)
	{
		static_cast<CAreaManager*>(g_pIEntitySystem->GetAreaManager())->ExitAllAreas(m_nID);
	}

	//////////////////////////////////////////////////////////////////////////
	// release AI object and SmartObject
	{
		if (gEnv->pAISystem)
		{
			if (IAIObject* pAIObject = GetAIObject())
			{
				gEnv->pAISystem->GetAIObjectManager()->RemoveObject(m_aiObjectID);
			}
		}

		m_aiObjectID = INVALID_AIOBJECTID;
		m_flags &= ~ENTITY_FLAG_HAS_AI;
	}

	if (gEnv->pAISystem && gEnv->pAISystem->GetSmartObjectManager())
		gEnv->pAISystem->GetSmartObjectManager()->RemoveSmartObject(this);

	//////////////////////////////////////////////////////////////////////////
	// remove timers and listeners
	RemoveAllEventListeners();

	g_pIEntitySystem->RemoveTimerEvent(GetId(), -1);
	g_pIEntitySystem->RemoveEntityFromLayers(GetId());

	DetachAll();
	DetachThis(0);

	if (m_pSchematycObject)
	{
		gEnv->pSchematyc->DestroyObject(m_pSchematycObject->GetId());
		m_pSchematycObject = nullptr;
		m_pSchematycProperties.reset();
	}

	// ShutDown all components.
	m_components.Clear();

	m_bInShutDown = false;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::EnableInheritXForm(bool bEnable)
{
	m_bNotInheritXform = !bEnable;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::AttachChild(IEntity* pChildEntity, const SChildAttachParams& attachParams)
{
	assert(pChildEntity);
	// In debug mode check for attachment recursion.
#ifdef _DEBUG
	assert(this != pChildEntity && "Trying to Attach to itself");
	for (IEntity* pParent = GetParent(); pParent; pParent = pParent->GetParent())
	{
		assert(pParent != pChildEntity && "Recursive Attachment");

	}
#endif
	if (pChildEntity == this)
	{
		EntityWarning("Trying to attaching Entity %s to itself", GetEntityTextDescription().c_str());
		return;
	}

	// Child entity must be also CEntity.
	CEntity* pChild = reinterpret_cast<CEntity*>(pChildEntity);
	PREFAST_ASSUME(pChild);

	// If not already attached to this node.
	if (pChild->m_hierarchy.pParent == this)
		return;

	Matrix34 childTM;
	if (attachParams.m_nAttachFlags & ATTACHMENT_KEEP_TRANSFORMATION)
	{
		childTM = pChild->GetWorldTM();
	}

	if (attachParams.m_nAttachFlags & ATTACHMENT_GEOMCACHENODE)
	{
#if defined(USE_GEOM_CACHES)
		pChild->m_hierarchy.parentBindingType = EBindingType::eBT_GeomCacheNode;
		CGeomCacheAttachmentManager* pGeomCacheAttachmentManager = static_cast<CEntitySystem*>(GetEntitySystem())->GetGeomCacheAttachmentManager();
		pGeomCacheAttachmentManager->RegisterAttachment(pChild, this, CryStringUtils::HashString(attachParams.m_target));
#endif
	}
	else if (attachParams.m_nAttachFlags & ATTACHMENT_CHARACTERBONE)
	{
		pChild->m_hierarchy.parentBindingType = EBindingType::eBT_CharacterBone;
		CCharacterBoneAttachmentManager* pCharacterBoneAttachmentManager = static_cast<CEntitySystem*>(GetEntitySystem())->GetCharacterBoneAttachmentManager();
		const uint32 targetCRC = CCrc32::ComputeLowercase(attachParams.m_target);
		pCharacterBoneAttachmentManager->RegisterAttachment(pChild, this, targetCRC);
	}
	else if (attachParams.m_nAttachFlags & ATTACHMENT_LOCAL_SIM)
	{
		pChild->m_hierarchy.parentBindingType = EBindingType::eBT_LocalSim;
	}

	// Add to child list first to make sure node not get deleted while re-attaching.
	m_hierarchy.childs.push_back(pChild);
	if (pChild->m_hierarchy.pParent)
		pChild->DetachThis(); // Detach node if attached to other parent.

	// Assign this entity as parent to child entity.
	pChild->m_hierarchy.pParent = this;

	if (attachParams.m_nAttachFlags & ATTACHMENT_SUPPRESS_UPDATE)
		return;

	if (attachParams.m_nAttachFlags & ATTACHMENT_KEEP_TRANSFORMATION)
	{
		// Keep old world space transformation.
		pChild->SetWorldTM(childTM);
	}
	else if (pChild->m_flags & ENTITY_FLAG_TRIGGER_AREAS)
	{
		// If entity should trigger areas, when attaching it make sure local translation is reset to (0,0,0).
		// This prevents invalid position to propagate to area manager and incorrectly leave/enter areas.
		pChild->m_vPos.Set(0, 0, 0);
		pChild->InvalidateTM(ENTITY_XFORM_FROM_PARENT);
	}
	else
		pChild->InvalidateTM(ENTITY_XFORM_FROM_PARENT);

	// Send attach event.
	SEntityEvent event(ENTITY_EVENT_ATTACH);
	event.nParam[0] = pChild->GetId();
	SendEvent(event);

	// Send ATTACH_THIS event to child
	SEntityEvent eventThis(ENTITY_EVENT_ATTACH_THIS);
	eventThis.nParam[0] = GetId();
	pChild->SendEvent(eventThis);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::DetachAll(int nDetachFlags)
{
	while (!m_hierarchy.childs.empty())
	{
		CEntity* pChild = m_hierarchy.childs.front();
		pChild->DetachThis(nDetachFlags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::DetachThis(int nDetachFlags, int nWhyFlags)
{
	if (m_hierarchy.pParent)
	{
		Matrix34 worldTM;

		bool bKeepTransform = (nDetachFlags & ATTACHMENT_KEEP_TRANSFORMATION) || (m_flags & ENTITY_FLAG_TRIGGER_AREAS);

		if (bKeepTransform)
		{
			worldTM = GetWorldTM();
		}

		// Copy parent to temp var, erasing child from parent may delete this node if child referenced only from parent.
		CEntity* pParent = m_hierarchy.pParent;
		m_hierarchy.pParent = NULL;

		// Remove child pointer from parent array of childs.
		stl::find_and_erase(pParent->m_hierarchy.childs, this);

		if (nDetachFlags & ATTACHMENT_SUPPRESS_UPDATE)
			return;

		if (bKeepTransform)
		{
			// Keep old world space transformation.
			SetLocalTM(worldTM, nWhyFlags | ENTITY_XFORM_FROM_PARENT);
		}
		else
			InvalidateTM(nWhyFlags | ENTITY_XFORM_FROM_PARENT);

		// Send detach event to parent.
		SEntityEvent event(ENTITY_EVENT_DETACH);
		event.nParam[0] = GetId();
		pParent->SendEvent(event);

		// Send DETACH_THIS event to child
		SEntityEvent eventThis(ENTITY_EVENT_DETACH_THIS);
		eventThis.nParam[0] = pParent->GetId();
		SendEvent(eventThis);

		if (m_hierarchy.parentBindingType == EBindingType::eBT_GeomCacheNode)
		{
#if defined(USE_GEOM_CACHES)
			CGeomCacheAttachmentManager* pGeomCacheAttachmentManager = static_cast<CEntitySystem*>(GetEntitySystem())->GetGeomCacheAttachmentManager();
			pGeomCacheAttachmentManager->UnregisterAttachment(this, pParent);
#endif
		}
		else if (m_hierarchy.parentBindingType == EBindingType::eBT_CharacterBone)
		{
			CCharacterBoneAttachmentManager* pCharacterBoneAttachmentManager = static_cast<CEntitySystem*>(GetEntitySystem())->GetCharacterBoneAttachmentManager();
			pCharacterBoneAttachmentManager->UnregisterAttachment(this, pParent);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CEntity::GetChildCount() const
{
	return m_hierarchy.childs.size();
}

//////////////////////////////////////////////////////////////////////////
IEntity* CEntity::GetChild(int nIndex) const
{
	if (nIndex >= 0 && nIndex < (int)m_hierarchy.childs.size())
		return m_hierarchy.childs[nIndex];

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetWorldTM(const Matrix34& tm, int nWhyFlags)
{
	if (GetParent() && !m_bNotInheritXform)
	{
		Matrix34 localTM = GetParentAttachPointWorldTM().GetInverted() * tm;
		SetLocalTM(localTM, nWhyFlags);
	}
	else
	{
		SetLocalTM(tm, nWhyFlags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetLocalTM(const Matrix34& tm, int nWhyFlags)
{
	if (m_bNotInheritXform && (nWhyFlags & ENTITY_XFORM_FROM_PARENT)) // Ignore parent x-forms.
		return;

	if (tm.IsOrthonormal())
		SetPosRotScale(tm.GetTranslation(), Quat(tm), Vec3(1.0f, 1.0f, 1.0f), nWhyFlags);
	else
	{
		AffineParts affineParts;
		affineParts.SpectralDecompose(tm);

		SetPosRotScale(affineParts.pos, affineParts.rot, affineParts.scale, nWhyFlags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetPosRotScale(const Vec3& vPos, const Quat& qRotation, const Vec3& vScale, int nWhyFlags)
{
	int changed = 0;

	if (m_bNotInheritXform && (nWhyFlags & ENTITY_XFORM_FROM_PARENT)) // Ignore parent x-forms.
		return;

	if (CheckFlags(ENTITY_FLAG_IGNORE_PHYSICS_UPDATE) && (nWhyFlags & ENTITY_XFORM_PHYSICS_STEP)) // Ignore physical based position in editor related entities
	{
		return;
	}

	if (m_vPos != vPos)
	{
		nWhyFlags |= ENTITY_XFORM_POS;
		changed++;
		CHECKQNAN_VEC(vPos);
		m_vPos = vPos;
	}

	if (m_qRotation.v != qRotation.v || m_qRotation.w != qRotation.w)
	{
		nWhyFlags |= ENTITY_XFORM_ROT;
		changed++;
		m_qRotation = qRotation;
	}

	if (m_vScale != vScale)
	{
		nWhyFlags |= ENTITY_XFORM_SCL;
		changed++;
		m_vScale = vScale;
	}

	if (changed != 0)
	{
		InvalidateTM(nWhyFlags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::CalcLocalTM(Matrix34& tm) const
{
	tm.Set(m_vScale, m_qRotation, m_vPos);
}

//////////////////////////////////////////////////////////////////////////
Matrix34 CEntity::GetLocalTM() const
{
	Matrix34 tm;
	CalcLocalTM(tm);
	return tm;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::AddEntityEventListener(EEntityEvent event, IEntityEventListener* pListener)
{
	if (!m_pEventListeners)
	{
		m_pEventListeners.reset(new SEventListeners(1));
	}
	m_pEventListeners->haveListenersMask |= (BIT64(event));
	m_pEventListeners->m_listeners.Add(SEventListeners::SListener(event, pListener));
}

//////////////////////////////////////////////////////////////////////////
void CEntity::RemoveEntityEventListener(EEntityEvent event, IEntityEventListener* pListener)
{
	if (m_pEventListeners)
		m_pEventListeners->m_listeners.Remove(SEventListeners::SListener(event, pListener));
}

//////////////////////////////////////////////////////////////////////////
void CEntity::RemoveAllEventListeners()
{
	m_pEventListeners.reset();
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::IsRendered() const
{
	return m_render.IsRendered();
}

void CEntity::PreviewRender(SEntityPreviewContext& context)
{
	m_render.PreviewRender(context);

	m_components.ForEach([&](const SEntityComponentRecord& rec)
	{
		IEntityComponentPreviewer* pPreviewer = rec.pComponent->GetPreviewer();
		if (pPreviewer)
		{
			pPreviewer->Render(*this, *rec.pComponent.get(), context);
		}
	});
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnRellocate(int nWhyFlags)
{
	if (m_flags & ENTITY_FLAG_TRIGGER_AREAS && (nWhyFlags & ENTITY_XFORM_POS))
	{
		static_cast<CAreaManager*>(g_pIEntitySystem->GetAreaManager())->MarkEntityForUpdate(GetId());
	}

	//////////////////////////////////////////////////////////////////////////
	// Reposition entity in the partition grid.
	//////////////////////////////////////////////////////////////////////////
	if ((m_flags & ENTITY_FLAG_NO_PROXIMITY) == 0 &&
	    (!m_bHidden || (nWhyFlags & ENTITY_XFORM_EDITOR))
	    && !m_bInShutDown)
	{
		if (nWhyFlags & ENTITY_XFORM_POS)
		{
			m_bWasRelocated = true;
			Vec3 wp = GetWorldPos();
			m_pGridLocation = g_pIEntitySystem->GetPartitionGrid()->Rellocate(m_pGridLocation, wp, this);
			if (m_pGridLocation)
				m_pGridLocation->nEntityFlags = m_flags;

			if (!m_bTrigger)
			{
				if (!m_pProximityEntity)
					m_pProximityEntity = g_pIEntitySystem->GetProximityTriggerSystem()->CreateEntity(m_nID);
				g_pIEntitySystem->GetProximityTriggerSystem()->MoveEntity(m_pProximityEntity, wp);
			}
		}
	}
	else
	{
		if (m_pGridLocation)
		{
			m_bWasRelocated = true;
			g_pIEntitySystem->GetPartitionGrid()->FreeLocation(m_pGridLocation);
			m_pGridLocation = 0;
		}

		if (m_bHidden)
		{
			if (!m_bTrigger)
			{
				//////////////////////////////////////////////////////////////////////////
				if (m_pProximityEntity && !(m_flags & ENTITY_FLAG_LOCAL_PLAYER)) // Hidden local player still should trigger proximity triggers in editor.
				{
					g_pIEntitySystem->GetProximityTriggerSystem()->RemoveEntity(m_pProximityEntity, true);
					m_pProximityEntity = 0;
				}
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////
}

IMaterial* CEntity::GetRenderMaterial(int nSlot) const
{
	return m_render.GetRenderMaterial(nSlot);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::InvalidateTM(int nWhyFlags, bool bRecalcPhyBounds)
{
	if (nWhyFlags & ENTITY_XFORM_FROM_PARENT)
	{
		nWhyFlags |= ENTITY_XFORM_POS | ENTITY_XFORM_ROT | ENTITY_XFORM_SCL;
	}

	CalcLocalTM(m_worldTM);

	if (m_hierarchy.pParent && !m_bNotInheritXform)
	{
		m_worldTM = GetParentAttachPointWorldTM() * m_worldTM;
	}

	// Invalidate matrices for all child objects.
	std::vector<CEntity*>::const_iterator it = m_hierarchy.childs.begin();
	const std::vector<CEntity*>::const_iterator end = m_hierarchy.childs.end();
	CEntity* pChild = NULL;
	for (; it != end; ++it)
	{
		pChild = *it;
		if (pChild)
		{
			pChild->InvalidateTM(nWhyFlags | ENTITY_XFORM_FROM_PARENT);
		}
	}

	// [*DavidR | 23/Sep/2008] Caching world-space forward dir ignoring scaling
	if (nWhyFlags & (ENTITY_XFORM_SCL | ENTITY_XFORM_ROT))
	{
		m_bDirtyForwardDir = true;
	}

	OnRellocate(nWhyFlags);

	// Send transform event.
	if (!(nWhyFlags & ENTITY_XFORM_NO_EVENT))
	{
		SEntityEvent event(ENTITY_EVENT_XFORM);
		event.nParam[0] = nWhyFlags;
#ifdef SEG_WORLD
		event.nParam[1] = bRecalcPhyBounds;
#endif
		SendEvent(event);
	}
}

//////////////////////////////////////////////////////////////////////////
IScriptTable* CEntity::GetScriptTable() const
{
	IEntityScriptComponent* pScriptProxy = (IEntityScriptComponent*)GetProxy(ENTITY_PROXY_SCRIPT);
	if (pScriptProxy)
		return pScriptProxy->GetScriptTable();
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetPos(const Vec3& vPos, int nWhyFlags, bool bRecalcPhyBounds, bool bForce)
{
	CHECKQNAN_VEC(vPos);
	if (m_vPos == vPos)
		return;

	m_vPos = vPos;
	InvalidateTM(nWhyFlags | ENTITY_XFORM_POS, bRecalcPhyBounds); // Position change.
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetRotation(const Quat& qRotation, int nWhyFlags)
{
	if (m_qRotation.v == qRotation.v && m_qRotation.w == qRotation.w)
		return;

	m_qRotation = qRotation;
	InvalidateTM(nWhyFlags | ENTITY_XFORM_ROT); // Rotation change.
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetScale(const Vec3& vScale, int nWhyFlags)
{
	if (m_vScale == vScale)
		return;

	m_vScale = vScale;
	InvalidateTM(nWhyFlags | ENTITY_XFORM_SCL); // Scale change.
}

//////////////////////////////////////////////////////////////////////////
Ang3 CEntity::GetWorldAngles() const
{
	if (IsScaled())
	{
		Matrix34 tm = GetWorldTM();
		tm.OrthonormalizeFast();
		Ang3 angles = Ang3::GetAnglesXYZ(tm);
		return angles;
	}
	else
	{
		Ang3 angles = Ang3::GetAnglesXYZ(GetWorldTM());
		return angles;
	}
}

//////////////////////////////////////////////////////////////////////////
Quat CEntity::GetWorldRotation() const
{
	//	if (m_vScale == Vec3(1,1,1))
	if (IsScaled())
	{
		Matrix34 tm = GetWorldTM();
		tm.OrthonormalizeFast();
		return Quat(tm);
	}
	else
	{
		return Quat(GetWorldTM());
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GetWorldBounds(AABB& bbox) const
{
	GetLocalBounds(bbox);
	bbox.SetTransformedAABB(GetWorldTM(), bbox);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GetLocalBounds(AABB& bbox) const
{
	bbox.min.Set(0, 0, 0);
	bbox.max.Set(0, 0, 0);

	if (GetSlotCount() > 0 || m_render.m_bBoundsFixed)
	{
		m_render.GetLocalBounds(bbox);
	}
	else
	{
		IEntityTriggerComponent* pTriggerProxy = (IEntityTriggerComponent*)GetProxy(ENTITY_PROXY_TRIGGER);
		if (pTriggerProxy)
		{
			pTriggerProxy->GetTriggerBounds(bbox);
		}
		else
		{
			m_physics.GetLocalBounds(bbox);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetLocalBounds(const AABB& bounds, bool bDoNotRecalculate)
{
	m_render.SetLocalBounds(bounds, bDoNotRecalculate);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::InvalidateLocalBounds()
{
	m_render.InvalidateLocalBounds();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::ActivateEntityIfNecessary()
{
	bool bEnable = ShouldActivate();

	if (bEnable != m_bInActiveList)
	{
		g_pIEntitySystem->ActivateEntity(this, bEnable);

		if (IAIObject* pAIObject = GetAIObject())
			if (IAIActorProxy* pProxy = pAIObject->GetProxy())
				pProxy->EnableUpdate(bEnable);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::ShouldActivate()
{
	bool bActivateByPhysics = false;

	EEntityUpdatePolicy policy = (EEntityUpdatePolicy)m_eUpdatePolicy;
	// If its update depends on physics, physics state defines if this entity is to be updated.
	if (policy == ENTITY_UPDATE_PHYSICS || policy == ENTITY_UPDATE_PHYSICS_VISIBLE)
	{
		if ((m_physics.m_nFlags & CEntityPhysics::FLAG_ACTIVE) != 0)
		{
			bActivateByPhysics = true;
		}
	}

	return (m_bRequiresComponentUpdate || m_nUpdateCounter || bActivateByPhysics) &&
	       (!m_bHidden || CheckFlags(ENTITY_FLAG_UPDATE_HIDDEN));
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UpdateComponentEventMask(const IEntityComponent* pComponent)
{
	m_components.ForEach([this, pComponent](SEntityComponentRecord& record)
	{
		if (record.pComponent.get() == pComponent)
		{
		  record.registeredEventsMask = record.pComponent->GetEventMask();

		  // Check if the remaining components are still interested in updates
		  m_bRequiresComponentUpdate = 0;

		  for (const SEntityComponentRecord& componentRecord : m_components.GetVector())
		  {
		    if (componentRecord.pComponent && (componentRecord.registeredEventsMask & BIT64(ENTITY_EVENT_UPDATE)) != 0)
		    {
		      m_bRequiresComponentUpdate = 1;
		      break;
		    }
		  }

		  OnComponentMaskChanged(*record.pComponent, record.registeredEventsMask);
		  return;
		}
	});
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnComponentMaskChanged(const IEntityComponent& component, uint64 newMask)
{
	if (newMask & BIT64(ENTITY_EVENT_RENDER_VISIBILITY_CHANGE))
	{
		// If any component want to process ENTITY_EVENT_RENDER_VISIBILITY_CHANGE we have to enable ENTITY_FLAG_SEND_RENDER_EVENT flag on the entity
		SetFlags(GetFlags() | ENTITY_FLAG_SEND_RENDER_EVENT);
	}

	if (newMask & BIT64(ENTITY_EVENT_PREPHYSICSUPDATE))
	{
		// If component want to receive ENTITY_EVENT_PREPHYSICSUPDATE, we must mark this entity to be able to send it.
		PrePhysicsActivate(true);
	}

	if (m_bRequiresComponentUpdate == 0 && newMask & BIT64(ENTITY_EVENT_UPDATE))
	{
		m_bRequiresComponentUpdate = 1;
	}
	ActivateEntityIfNecessary();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::PrePhysicsActivate(bool bActive)
{
	g_pIEntitySystem->ActivatePrePhysicsUpdateForEntity(this, bActive);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetTimer(int nTimerId, int nMilliSeconds)
{
	KillTimer(nTimerId);
	SEntityTimerEvent timeEvent;
	timeEvent.entityId = m_nID;
	timeEvent.nTimerId = nTimerId;
	timeEvent.nMilliSeconds = nMilliSeconds;
	g_pIEntitySystem->AddTimerEvent(timeEvent);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::KillTimer(int nTimerId)
{
	g_pIEntitySystem->RemoveTimerEvent(m_nID, nTimerId);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Hide(bool bHide, EEntityHideFlags hideFlags)
{
	if ((bool)m_bHidden != bHide)
	{
		m_bHidden = (unsigned int)bHide;

		// Update registered locations
		OnRellocate(ENTITY_XFORM_POS);

		SendHideEvent(bHide, hideFlags);

		ActivateEntityIfNecessary();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SendHideEvent(bool bHide, EEntityHideFlags hideFlags)
{
	SEntityEvent e(bHide ? ENTITY_EVENT_HIDE : ENTITY_EVENT_UNHIDE);
	e.nParam[0] = hideFlags;
	SendEvent(e);

	// Propagate Hide flag to the child entities.
	for (int i = 0; i < (int)m_hierarchy.childs.size(); i++)
	{
		if (m_hierarchy.childs[i] != NULL)
		{
			m_hierarchy.childs[i]->Hide(bHide, static_cast<EEntityHideFlags>(hideFlags | ENTITY_HIDE_PARENT));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Invisible(bool bInvisible)
{
	if ((bool)m_bInvisible != bInvisible)
	{
		m_bInvisible = (unsigned int)bInvisible;

		if (bInvisible)
		{
			SEntityEvent e(ENTITY_EVENT_INVISIBLE);
			SendEvent(e);
		}
		else
		{
			SEntityEvent e(ENTITY_EVENT_VISIBLE);
			SendEvent(e);
		}

		// Propagate invisible flag to the child entities.
		for (int i = 0; i < (int)m_hierarchy.childs.size(); i++)
		{
			if (m_hierarchy.childs[i] != NULL)
			{
				m_hierarchy.childs[i]->Invisible(bInvisible);
			}

		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetUpdatePolicy(EEntityUpdatePolicy eUpdatePolicy)
{
	m_eUpdatePolicy = eUpdatePolicy;
}

//////////////////////////////////////////////////////////////////////////
string CEntity::GetEntityTextDescription() const
{
	return m_szName + " (" + m_pClass->GetName() + ")";
}

//////////////////////////////////////////////////////////////////////////
void CEntity::LoadComponent(Serialization::IArchive& archive)
{
	// Load component Type GUID
	CryGUID typeGUID;

	// Load class GUID
	archive(typeGUID, "TypeGUID", "TypeGUID");

	// Load component name attribute
	string name;
	archive(name, "Name", "Name");

	const Schematyc::IEnvComponent* pEnvComponent = gEnv->pSchematyc->GetEnvRegistry().GetComponent(typeGUID);
	if (!pEnvComponent)
	{
		char guidStr[128];
		typeGUID.ToString(guidStr);
		// Unknown Entity Component
		EntityWarning("Attempting to load unknown Entity Component: %s {%s}", name.c_str(), guidStr);
		return;
	}

	const CEntityComponentClassDesc& componentClassDesc = pEnvComponent->GetDesc();
	EntityComponentFlags componentFlags = componentClassDesc.GetComponentFlags();
	if ((componentFlags.Check(EEntityComponentFlags::ServerOnly) && !gEnv->bServer)
		|| (componentFlags.Check(EEntityComponentFlags::ClientOnly) && !gEnv->IsClient()))
	{
		return;
	}

	// Load component unique GUID
	CryGUID componentGUID;
	archive(componentGUID, "GUID", "GUID");

	std::shared_ptr<IEntityComponent> pComponent;
	IEntityComponent* pParentComponent = nullptr;

	bool bValidInstanceGUID = !componentGUID.IsNull() && (m_flagsExtended & ENTITY_FLAG_EXTENDED_CLONED) == 0;
	if (bValidInstanceGUID)
	{
		// Find component in the list of the existing entity components
		for (auto& record : m_components.GetVector())
		{
			if (record.pComponent != nullptr && record.pComponent->GetGUID() == componentGUID)
			{
				pComponent = record.pComponent;
				break;
			}
		}
	}
	else
	{
		// We might parse components with no GUID, however this should not occur so we create one here.
		componentGUID = CryGUID::Create();
	}

	// Try to find parent component
	CryGUID parentGUID;
	if (archive(parentGUID, "ParentGUID", "ParentGUID") && !parentGUID.IsNull())
	{
		pParentComponent = GetComponentByGUID(parentGUID);
	}

	// Load component transform
	CryTransform::CTransformPtr pTransform;
	CryTransform::CTransform transform;
	if (archive(transform, "Transform", "Transform"))
	{
		pTransform = std::make_shared<CryTransform::CTransform>(transform);
	}

	// Load user attribute
	bool bUserAdded = false;
	if (archive(bUserAdded, "UserAdded", "UserAdded"))
	{
		if (bUserAdded)
		{
			componentFlags.Add(EEntityComponentFlags::UserAdded);
		}
	}

	//Schematyc::CClassProperties classProperties;
	//classProperties.Set(componentClassDesc);
	//classProperties.SetOverridePolicy(Schematyc::EOverridePolicy::Override);
	//archive(classProperties,"properties","properties");

	IEntityComponent::SInitParams initParams(this, componentGUID, name, &componentClassDesc, componentFlags, pParentComponent, pTransform);

	if (!pComponent)
	{
		pComponent = pEnvComponent->CreateFromPool();
		pComponent->PreInit(initParams);

		// Apply loaded properties values on the members of the component
		//classProperties.Apply(componentClassDesc, pComponent.get());
		Schematyc::Utils::SerializeClass(archive, componentClassDesc, pComponent.get(), "properties", "properties");

		// Add the component to the entity
		AddComponentInternal(pComponent, typeGUID, &initParams, &componentClassDesc);
	}
	else
	{
		if (&pComponent->GetClassDesc() != &componentClassDesc)
		{
			// Found by GUID component must have same class as ClassDesc from EnvComponent
			char guidStr[128];
			typeGUID.ToString(guidStr);
			EntityWarning("Attempting to load Entity Component with the wrong ClassDesc: %s {%s}", name.c_str(), guidStr);
			return;
		}
		pComponent->m_name = name;
		if (pTransform != nullptr)
		{
			pComponent->SetTransformMatrix(transform.ToMatrix34());
		}
		pComponent->SetComponentFlags(componentFlags);

		// Apply loaded properties values on the members of the component
		//classProperties.Apply(componentClassDesc, pComponent.get());
		Schematyc::Utils::SerializeClass(archive, componentClassDesc, pComponent.get(), "properties", "properties");
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SaveComponent(Serialization::IArchive& archive, IEntityComponent& component)
{
	// Load component Type GUID
	CryGUID typeGUID;
	CryGUID componentGUID;
	CryGUID parentGUID;
	EntityComponentFlags componentFlags;
	CryTransform::CTransformPtr pTransform;

	// Load class GUID
	archive(component.GetClassDesc().GetGUID(), "TypeGUID", "TypeGUID");

	string typestr = component.GetClassDesc().GetName().c_str();
	archive(typestr, "TypeName", "TypeName");

	// Load component unique GUID
	archive(component.GetGUID(), "GUID", "GUID");

	if (component.GetParent())
	{
		archive(component.GetParent()->GetGUID(), "ParentGUID", "ParentGUID");
	}

	// Load component name attribute
	string name = component.GetName();
	archive(name, "Name", "Name");

	// Load user attribute
	bool bUserAdded = component.GetComponentFlags().Check(EEntityComponentFlags::UserAdded);
	archive(bUserAdded, "UserAdded", "UserAdded");

	// Load component transform
	if (component.GetComponentFlags().Check(EEntityComponentFlags::Transform) && component.GetTransform())
	{
		archive(*component.GetTransform(), "Transform", "Transform");
	}

	//Schematyc::CClassProperties classProperties;
	//classProperties.Set(classDesc);
	//classProperties.SetOverridePolicy(Schematyc::EOverridePolicy::Override);
	//classProperties.Read(comp)
	//archive(classProperties, "properties", "properties");

	// Save component members
	Schematyc::Utils::SerializeClass(archive, component.GetClassDesc(), &component, "properties", "properties");
}

static bool SerializePropertiesWrapper(void* rawPointer, yasli::Archive& ar)
{
	static_cast<IEntityPropertyGroup*>(rawPointer)->SerializeProperties(ar);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::LoadComponentLegacy(XmlNodeRef& entityNode, XmlNodeRef& componentNode)
{
	//////////////////////////////////////////////////////////////////////////
	// Load component Type GUID
	CryInterfaceID componentTypeId;
	if (!componentNode->getAttr("typeId", componentTypeId))
	{
		return false;
	}

	IEntityComponent* pComponent = GetComponentByTypeId(componentTypeId);

	if (!pComponent)
	{
		for (const SEntityComponentRecord& record : m_components.GetVector())
		{
			if (record.pComponent != nullptr && record.pComponent->GetClassDesc().GetGUID() == componentTypeId)
			{
				pComponent = record.pComponent.get();
				break;
			}
		}
	}
	if (pComponent && pComponent->GetComponentFlags().Check(EEntityComponentFlags::Schematyc))
	{
		// Do not load Schematyc components
		return true;
	}

	if (!pComponent)
	{
		// Script proxy cannot be created from list of components.
		bool bCanCreateComponent = componentTypeId != CEntityComponentLuaScript::GetCID() &&
		                           componentTypeId != cryiidof<IEntityScriptComponent>() &&
		                           componentTypeId != cryiidof<ICryUnknown>() &&
		                           componentTypeId != cryiidof<IEntityComponent>();

		if (bCanCreateComponent)
		{
			// Only user created components, should create components, otherwise component should be created by entity class or Schematyc objects
			IEntityComponent::SInitParams initParams(this, CryGUID(), "", nullptr, EEntityComponentFlags::None, nullptr, nullptr);
			pComponent = CreateComponentByInterfaceID(componentTypeId, &initParams);
		}
	}

	if (pComponent)
	{
		// Parse component properties, if any
		IEntityPropertyGroup* pProperties = pComponent->GetPropertyGroup();
		if (pProperties)
		{
			gEnv->pSystem->GetArchiveHost()->LoadXmlNode(Serialization::SStruct(yasli::TypeID::get<IEntityPropertyGroup>(), (void*)pProperties, sizeof(IEntityPropertyGroup), &SerializePropertiesWrapper), componentNode);
		}
		else
		{
			// No property group, also try legacy serialization
			pComponent->LegacySerializeXML(entityNode, componentNode, true);
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SaveComponentLegacy(CryGUID typeId, XmlNodeRef& entityNode, XmlNodeRef& componentNode, IEntityComponent& component, bool bIncludeScriptProxy)
{
	componentNode->setAttr("typeId", typeId);
	if (!component.GetGUID().IsNull())
	{
		componentNode->setAttr("guid", component.GetGUID());
	}

	const char* szComponentName = component.GetName();

	if (szComponentName != nullptr && szComponentName[0] != '\0')
	{
		componentNode->setAttr("name", szComponentName);
	}

	bool bUserAdded = false;
	if (component.GetComponentFlags().Check(EEntityComponentFlags::UserAdded))
	{
		componentNode->setAttr("UserAdded", true);
		bUserAdded = true;
	}

	// Legacy type of serialization
	if (component.GetProxyType() != ENTITY_PROXY_SCRIPT || bIncludeScriptProxy)
	{
		component.LegacySerializeXML(entityNode, componentNode, false);
	}

	IEntityPropertyGroup* pProperties = component.GetPropertyGroup();
	// Parse component properties, if any
	if (pProperties)
	{
		gEnv->pSystem->GetArchiveHost()->SaveXmlNode(componentNode, Serialization::SStruct(yasli::TypeID::get<IEntityPropertyGroup>(), (void*)pProperties, sizeof(IEntityPropertyGroup), &SerializePropertiesWrapper));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SerializeXML(XmlNodeRef& node, bool bLoading, bool bIncludeScriptProxy)
{
	m_physics.SerializeXML(node, bLoading);

	if (bLoading)
	{
		if (XmlNodeRef componentsNode = node->findChild("Components"))
		{
			// Load components into the m_components vector (without initializing them!)
			for (int i = 0, n = componentsNode->getChildCount(); i < n; ++i)
			{
				XmlNodeRef componentNode = componentsNode->getChild(i);
				if (componentNode->haveAttr("typeId"))
				{
					LoadComponentLegacy(node, componentNode);
				}
				else
				{
					struct SEntityComponentLoadHelper
					{
						CEntity& entity;
						void Serialize(Serialization::IArchive& archive)
						{
							entity.LoadComponent(archive);
						}
					};

					SEntityComponentLoadHelper loadHelper = SEntityComponentLoadHelper{ *this };
					Serialization::LoadXmlNode(loadHelper, componentNode);
				}
			}

			// Now that all components are in place, initialize them (does not apply to legacy proxies).
			m_components.ForEach([&](const SEntityComponentRecord& record)
			{
				if (record.proxyType == ENTITY_PROXY_LAST)
				{
					record.pComponent->Initialize();
				}
			});
		}
		m_physics.SerializeXML(node, bLoading);
	}
	else
	{
		XmlNodeRef componentsNode;

		m_components.ForEach([&](const SEntityComponentRecord& record)
		{
			IEntityComponent& component = *record.pComponent;

			// Skip all components created by Schematyc
			if (!component.GetComponentFlags().Check(EEntityComponentFlags::Schematyc) &&
					!component.GetComponentFlags().Check(EEntityComponentFlags::NoSave))
			{
				if (!component.GetClassDesc().GetName().IsEmpty())
				{
					if (!componentsNode)
					{
						// Create sub-node on first access
						componentsNode = node->newChild("Components");
					}
					XmlNodeRef componentNode = componentsNode->newChild("Component");

					struct SEntityComponentSaveHelper
					{
						CEntity&          entity;
						IEntityComponent* pComponent;
						void Serialize(Serialization::IArchive& archive)
						{
							entity.SaveComponent(archive, *pComponent);
						}
					};

					SEntityComponentSaveHelper saveHelper{ *this, &component };
					Serialization::SaveXmlNode(componentNode, Serialization::SStruct(saveHelper));
				}
				else if (component.GetPropertyGroup() || component.GetComponentFlags().Check(EEntityComponentFlags::Legacy))
				{
					if (!componentsNode)
					{
						// Create sub-node on first access
						componentsNode = node->newChild("Components");
					}
					XmlNodeRef componentNode = componentsNode->newChild("Component");
					SaveComponentLegacy(record.typeId, node, componentNode, component, bIncludeScriptProxy);
				}
			}
		});

		if (m_pSchematycProperties)
		{
			// Save Schematyc object properties from the Entity Node XML data
			XmlNodeRef schematycPropsNode = node->newChild("SchematycProperties");
			Serialization::SaveXmlNode(schematycPropsNode, *m_pSchematycProperties.get());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IEntityComponent* CEntity::GetProxy(EEntityProxy proxy) const
{
	for (const SEntityComponentRecord& componentRec : m_components.GetVector())
	{
		if (componentRec.proxyType == proxy)
			return componentRec.pComponent.get();
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
IEntityComponent* CEntity::CreateProxy(EEntityProxy proxy)
{
	for (const SEntityComponentRecord& componentRec : m_components.GetVector())
	{
		if (componentRec.proxyType == proxy)
			return componentRec.pComponent.get();
	}

	switch (proxy)
	{
	case ENTITY_PROXY_AUDIO:
		return CreateComponent<CEntityComponentAudio>();
		break;
	case ENTITY_PROXY_AREA:
		return CreateComponent<CEntityComponentArea>();
		break;
	case ENTITY_PROXY_FLOWGRAPH:
		return CreateComponent<CEntityComponentFlowGraph>();
		break;
	case ENTITY_PROXY_SUBSTITUTION:
		return CreateComponent<CEntityComponentSubstitution>();
		break;
	case ENTITY_PROXY_TRIGGER:
		return CreateComponent<CEntityComponentTriggerBounds>();
		break;
	case ENTITY_PROXY_CAMERA:
		return CreateComponent<CEntityComponentCamera>();
		break;
	case ENTITY_PROXY_ROPE:
		return CreateComponent<CEntityComponentRope>();
		break;
	case ENTITY_PROXY_ENTITYNODE:
		return CreateComponent<CEntityComponentTrackViewNode>();
		break;
	case ENTITY_PROXY_CLIPVOLUME:
		return CreateComponent<CEntityComponentClipVolume>();
		break;
	case ENTITY_PROXY_DYNAMICRESPONSE:
		return CreateComponent<CEntityComponentDynamicResponse>();
		break;
	}
	assert(0);
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
IEntityComponent* CEntity::CreateComponentByInterfaceID(const CryInterfaceID& interfaceId, IEntityComponent::SInitParams *pInitParams)
{
	const CEntityComponentClassDesc* pClassDescription = nullptr;
	CryGUID componentTypeID;

	// First look for a unified Schematyc / Entity component type
	// TODO: Search hierarchy
	const Schematyc::IEnvComponent* pEnvComponent = gEnv->pSchematyc->GetEnvRegistry().GetComponent(interfaceId);
	ICryFactory* pLegacyComponentFactory = nullptr;
	if (pEnvComponent != nullptr)
	{
		// Resolve to implementation ID
		componentTypeID = pEnvComponent->GetGUID();
		pClassDescription = &pEnvComponent->GetDesc();
	}
	// Fall back to legacy 5.3 creation
	else if (ICryFactoryRegistry* pFactoryRegistry = gEnv->pSystem->GetCryFactoryRegistry())
	{
		size_t numFactories = 1;
		pFactoryRegistry->IterateFactories(interfaceId, &pLegacyComponentFactory, numFactories);
		if (numFactories == 0 || pLegacyComponentFactory == nullptr)
		{
			// Nothing found by interface, check by implementation id
			pLegacyComponentFactory = pFactoryRegistry->GetFactory(interfaceId);
		}

		if (pLegacyComponentFactory == nullptr || !pLegacyComponentFactory->ClassSupports(cryiidof<IEntityComponent>()))
		{
			CRY_ASSERT_MESSAGE(0, "No component implementation registered for the given component interface");
			return nullptr;
		}

		// Resolve to implementation id, since we may have queried by interface
		componentTypeID = pLegacyComponentFactory->GetClassID();
	}
	else
	{
		CRY_ASSERT_MESSAGE("Tried to create unregistered component with type id %s!", interfaceId.ToDebugString());
		return nullptr;
	}

	// All pre-checks successful, we can now create the component
	// There can never be any failures after this point, we must always add the component to storage
	std::shared_ptr<IEntityComponent> pComponent = pEnvComponent != nullptr ? pEnvComponent->CreateFromPool() : cryinterface_cast<IEntityComponent>(pLegacyComponentFactory->CreateClassInstance());
	CRY_ASSERT(pComponent != nullptr);

	// Add the component to the entity, and initialize it manually immediately after
	AddComponentInternal(pComponent, componentTypeID, pInitParams, pClassDescription);

	pComponent->Initialize();

	return pComponent.get();
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::AddComponent(std::shared_ptr<IEntityComponent> pComponent, IEntityComponent::SInitParams *pInitParams)
{
	const CEntityComponentClassDesc* pClassDescription = pInitParams != nullptr ? pInitParams->classDesc : nullptr;
	if (pClassDescription != nullptr)
	{
		AddComponentInternal(pComponent, pClassDescription->GetGUID(), pInitParams, pClassDescription);
		return true;
	}
	else if(ICryFactory* pFactory = pComponent->GetFactory())
	{
		AddComponentInternal(pComponent, pFactory->GetClassID(), pInitParams, nullptr);
		return true;
	}

	AddComponentInternal(pComponent, CryGUID::Null(), pInitParams, nullptr);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::AddComponentInternal(std::shared_ptr<IEntityComponent> pComponent, const CryGUID& componentTypeID, IEntityComponent::SInitParams *pInitParams, const CEntityComponentClassDesc* pClassDescription)
{
	CRY_ASSERT_MESSAGE(pClassDescription == nullptr || !(pClassDescription->GetComponentFlags().Check(EEntityComponentFlags::ClientOnly) && !gEnv->IsClient()), "Trying to add a client-only component on the server!");
	CRY_ASSERT_MESSAGE(pClassDescription == nullptr || !(pClassDescription->GetComponentFlags().Check(EEntityComponentFlags::ServerOnly) && !gEnv->bServer), "Trying to add a server-only component on the client!");

	// Initialize common component members
	pComponent->PreInit(pInitParams != nullptr ? *pInitParams : IEntityComponent::SInitParams(this, CryGUID::Create(), "", pClassDescription, EEntityComponentFlags::None, nullptr, nullptr));

	// Initialize component entity pointer
	pComponent->m_pEntity = this;

	SEntityComponentRecord componentRecord;
	componentRecord.pComponent = pComponent;
	componentRecord.typeId = componentTypeID;
	componentRecord.registeredEventsMask = pComponent->GetEventMask();
	componentRecord.proxyType = (int)pComponent->GetProxyType();
	componentRecord.eventPriority = pComponent->GetEventPriority();

	// Automatically assign transformation if necessary
	if (pComponent->GetComponentFlags().Check(EEntityComponentFlags::Transform) && pComponent->GetTransform() == nullptr)
	{
		pComponent->SetTransformMatrix(IDENTITY);
	}

	OnComponentMaskChanged(*pComponent, componentRecord.registeredEventsMask);

	// Proxy component must be last in the order of the event processing
	if (componentRecord.proxyType == ENTITY_PROXY_SCRIPT)
		componentRecord.eventPriority = 10000;

	// Sorted insertion, all elements of the m_components are sorted by the proxyType
	m_components.Add(componentRecord);

	// Entity has changed so make the state dirty
	m_componentChangeState++;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::RemoveComponent(IEntityComponent* pComponent)
{
	m_components.Remove(pComponent);

	// Check if the remaining components are still interested in updates
	m_bRequiresComponentUpdate = 0;

	for (const SEntityComponentRecord& componentRecord : m_components.GetVector())
	{
		if (componentRecord.registeredEventsMask & BIT64(ENTITY_EVENT_UPDATE))
		{
			m_bRequiresComponentUpdate = 1;

			break;
		}
	}

	ActivateEntityIfNecessary();

	// Entity has changed so make the state dirty
	m_componentChangeState++;
}

void CEntity::RemoveAllComponents()
{
	m_components.Clear();

	// Entity has changed so make the state dirty
	m_componentChangeState++;
}

//////////////////////////////////////////////////////////////////////////
IEntityComponent* CEntity::GetComponentByTypeId(const CryInterfaceID& typeId) const
{
	for (const SEntityComponentRecord& componentRecord : m_components.GetVector())
	{
		if (componentRecord.typeId == typeId)
		{
			return componentRecord.pComponent.get();
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GetComponentsByTypeId(const CryInterfaceID& typeId, DynArray<IEntityComponent*>& components) const
{
	for (const SEntityComponentRecord& componentRecord : m_components.GetVector())
	{
		if (componentRecord.typeId == typeId)
		{
			components.push_back(componentRecord.pComponent.get());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IEntityComponent* CEntity::GetComponentByGUID(const CryGUID& guid) const
{
	for (auto& record : m_components.GetVector())
	{
		if (record.pComponent != nullptr && record.pComponent->GetGUID() == guid)
		{
			return record.pComponent.get();
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::QueryComponentsByInterfaceID(const CryInterfaceID& interfaceID, DynArray<IEntityComponent*> &components) const
{
	CRY_ASSERT(!interfaceID.IsNull());
	CRY_ASSERT(interfaceID != cryiidof<ICryUnknown>());
	CRY_ASSERT(interfaceID != cryiidof<IEntityComponent>());

	for (const SEntityComponentRecord& record : m_components.GetVector())
	{
		if (record.pComponent == nullptr)
			continue;

		// Check unified Schematyc / Entity class hierarchy
		if (record.pComponent->GetClassDesc().GetGUID() == interfaceID || record.pComponent->GetClassDesc().FindBaseByTypeID(interfaceID) != nullptr)
		{
			components.push_back(record.pComponent.get());
			continue;
		}

		// Check legacy component class hierarchy
		if(ICryFactory* pFactory = record.pComponent->GetFactory())
		{
			if (pFactory->GetClassID() == interfaceID || pFactory->ClassSupports(interfaceID))
			{
				components.push_back(record.pComponent.get());
				continue;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IEntityComponent* CEntity::QueryComponentByInterfaceID(const CryInterfaceID& interfaceID) const
{
	CRY_ASSERT(!interfaceID.IsNull());

	if (interfaceID == cryiidof<ICryUnknown>() || interfaceID == cryiidof<IEntityComponent>())
	{
		return nullptr;
	}

	for (const SEntityComponentRecord& record : m_components.GetVector())
	{
		if (record.pComponent == nullptr)
			continue;

		// Check unified Schematyc / Entity class hierarchy
		if (record.pComponent->GetClassDesc().GetGUID() == interfaceID || record.pComponent->GetClassDesc().FindBaseByTypeID(interfaceID) != nullptr)
		{
			return record.pComponent.get();
		}

		// Check legacy component class hierarchy
		if (ICryFactory* pFactory = record.pComponent->GetFactory())
		{
			if (pFactory->GetClassID() == interfaceID || pFactory->ClassSupports(interfaceID))
			{
				return record.pComponent.get();
			}
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::CloneComponentsFrom(IEntity& otherEntity)
{
	static_cast<CEntity&>(otherEntity).m_components.ForEach([this](const SEntityComponentRecord& componentRecord)
	{
		IEntityComponent* pSourceComponent = componentRecord.pComponent.get();

		// Create a new component
		IEntityComponent::SInitParams initParams(this, CryGUID::Create(), pSourceComponent->GetName(), &pSourceComponent->GetClassDesc(), pSourceComponent->GetComponentFlags(), pSourceComponent->GetParent(), pSourceComponent->GetTransform());
		IEntityComponent* pNewComponent = CreateComponentByInterfaceID(componentRecord.typeId, &initParams);

		DynArray<char> propertyBuffer;
		// Save properties from the source to buffer
		gEnv->pSystem->GetArchiveHost()->SaveBinaryBuffer(propertyBuffer, Serialization::SStruct(IEntityComponent::SPropertySerializer{ pSourceComponent }));

		// Deserialize to the target
		gEnv->pSystem->GetArchiveHost()->LoadBinaryBuffer(Serialization::SStruct(IEntityComponent::SPropertySerializer{ pNewComponent }), propertyBuffer.data(), propertyBuffer.size());
	});
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GetComponents(DynArray<IEntityComponent*>& components) const
{
	auto& vec = m_components.GetVector();
	components.resize(0);
	components.reserve(vec.size());
	for (const SEntityComponentRecord& rec : vec)
	{
		if (rec.pComponent != nullptr)
		{
			components.push_back(rec.pComponent.get());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
uint32 CEntity::GetComponentsCount() const
{
	return m_components.Size();
}

uint8 CEntity::GetComponentChangeState() const
{
	return m_componentChangeState;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::VisitComponents(const ComponentsVisitor& visitor)
{
	m_components.ForEach([&](const SEntityComponentRecord& componentRecord)
	{
		// Call visitor callback on every component
		visitor(componentRecord.pComponent.get());
	});
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Serialize(TSerialize ser, int nFlags)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Entity serialization");
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "%s", GetClass() ? GetClass()->GetName() : "<UNKNOWN>");
	if (nFlags & ENTITY_SERIALIZE_POSITION)
	{
		ser.Value("position", m_vPos, 'wrld');
	}
	if (nFlags & ENTITY_SERIALIZE_SCALE)
	{
		// TODO: check dimensions
		// TODO: define an AC_WorldPos type
		// TODO: check dimensions
		ser.Value("scale", m_vScale);
	}
	if (nFlags & ENTITY_SERIALIZE_ROTATION)
	{
		ser.Value("rotation", m_qRotation, 'ori3');
	}

	if (nFlags & ENTITY_SERIALIZE_GEOMETRIES)
	{
		int i, nSlots;
		IPhysicalEntity* pPhysics = GetPhysics();
		pe_params_part pp;
		bool bHasIt = false;
		float mass = 0;

		if (!ser.IsReading())
		{
			nSlots = GetSlotCount();
			ser.Value("numslots", nSlots);
			for (i = 0; i < nSlots; i++)
			{
				int nSlotId = i;
				CEntitySlot* const pSlot = GetSlot(i);
				if (ser.BeginOptionalGroup("Slot", (pSlot && pSlot->GetStatObj())))
				{
					ser.Value("id", nSlotId);
					bool bHasXfrom = !pSlot->GetLocalTM().IsIdentity();
					ser.Value("hasXform", bHasXfrom);
					if (bHasXfrom)
					{
						Vec3 col;
						col = pSlot->GetLocalTM().GetColumn(0);
						ser.Value("Xform0", col);
						col = pSlot->GetLocalTM().GetColumn(1);
						ser.Value("Xform1", col);
						col = pSlot->GetLocalTM().GetColumn(2);
						ser.Value("Xform2", col);
						col = pSlot->GetLocalTM().GetTranslation();
						ser.Value("XformT", col);
					}
					//ser.Value("hasmat", bHasIt=pSlot->pMaterial!=0);
					//if (bHasIt)
					//	ser.Value("matname", pSlot->pMaterial->GetName());
					uint32 slotFlags = pSlot->GetFlags();
					ser.Value("flags", slotFlags);
					bool bSlotUpdate = false;
					ser.Value("update", bSlotUpdate);
					if (pPhysics)
					{
						if (pPhysics->GetType() != PE_STATIC)
						{
							if (pSlot->GetStatObj()->GetFlags() & STATIC_OBJECT_COMPOUND)
							{
								MARK_UNUSED pp.partid;
								for (pp.ipart = 0, mass = 0; pPhysics->GetParams(&pp); pp.ipart++)
									mass += pp.mass;
							}
							else
							{
								pp.partid = i;
								MARK_UNUSED pp.ipart;
								if (pPhysics->GetParams(&pp))
									mass = pp.mass;
							}
						}
						ser.Value("mass", mass);
					}

					int bHeavySer = 0;
					if (pPhysics && pPhysics->GetType() == PE_RIGID)
						if (pSlot->GetStatObj()->GetFlags() & STATIC_OBJECT_COMPOUND)
						{
							IStatObj::SSubObject* pSubObj;
							for (i = 0; i < pSlot->GetStatObj()->GetSubObjectCount(); i++)
								if ((pSubObj = pSlot->GetStatObj()->GetSubObject(i)) && pSubObj->pStatObj)
									bHeavySer |= pSubObj->pStatObj->GetFlags() & STATIC_OBJECT_GENERATED;
						}
						else
							bHeavySer = pSlot->GetStatObj()->GetFlags() & STATIC_OBJECT_GENERATED;

					if (bHeavySer) // prevent heavy mesh serialization of modified statobjects
						ser.Value("noobj", bHasIt = true);
					else
					{
						ser.Value("noobj", bHasIt = false);
						gEnv->p3DEngine->SaveStatObj(pSlot->GetStatObj(), ser);
					}
					char maskBuf[sizeof(hidemaskLoc) * 2 + 2];
					hidemask hmask = pSlot->GetHidemask();
					ser.Value("sshidemask", _ui64toa(hmask, maskBuf, 16));
					pSlot->SetHidemask(hmask);
					ser.EndGroup(); //Slot
				}
			}
		}
		else
		{
			bool bClearAfterLoad = false;
			pe_action_remove_all_parts arp;
			if (pPhysics)
				pPhysics->Action(&arp);
			for (i = GetSlotCount() - 1; i >= 0; i--)
				FreeSlot(i);
			ser.Value("numslots", nSlots);
			for (int iter = 0; iter < nSlots; iter++)
			{
				int nSlotId = -1;
				if (ser.BeginOptionalGroup("Slot", true))
				{
					ser.Value("id", nSlotId);

					CEntitySlot* pSlot = m_render.AllocSlot(nSlotId);

					ser.Value("hasXform", bHasIt);
					if (bHasIt)
					{
						Vec3 col;
						Matrix34 mtx;
						ser.Value("Xform0", col);
						mtx.SetColumn(0, col);
						ser.Value("Xform1", col);
						mtx.SetColumn(1, col);
						ser.Value("Xform2", col);
						mtx.SetColumn(2, col);
						ser.Value("XformT", col);
						mtx.SetTranslation(col);
						SetSlotLocalTM(nSlotId, mtx);
					}

					uint32 slotFlags = pSlot->GetFlags();
					ser.Value("flags", slotFlags);
					pSlot->SetFlags(slotFlags);
					ser.Value("update", bHasIt);

					if (pPhysics)
						ser.Value("mass", mass);

					ser.Value("noobj", bHasIt);
					if (bHasIt)
						bClearAfterLoad = true;
					else
						SetStatObj(gEnv->p3DEngine->LoadStatObj(ser), ENTITY_SLOT_ACTUAL | nSlotId, pPhysics != 0, mass);

					string str;
					ser.Value("sshidemask", str);
					hidemaskLoc mask = 0;
					for (const char* ptr = str; *ptr; ptr++)
						(mask <<= 4) |= hidemaskLoc(*ptr - ('0' + ('a' - 10 - '0' & ('9' - *ptr) >> 8)));

					auto hmask = hidemask(mask);
					pSlot->SetHidemask(hmask);
					ser.EndGroup(); //Slot
				}
			}
			if (bClearAfterLoad)
			{
				if (pPhysics)
					pPhysics->Action(&arp);
				for (i = GetSlotCount() - 1; i >= 0; i--)
					FreeSlot(i);
			}
		}
	}

	if (nFlags & ENTITY_SERIALIZE_PROXIES)
	{
		//CryLogAlways("Serializing proxies for %s of class %s", GetName(), GetClass()->GetName());
		bool bSaveProxies = ser.GetSerializationTarget() == eST_Network; // always save for network stream
		if (!bSaveProxies && !ser.IsReading())
		{
			int proxyNrSerialized = 0;
			while (true)
			{
				EEntityProxy proxy = ProxySerializationOrder[proxyNrSerialized];
				if (proxy == ENTITY_PROXY_LAST)
					break;
				IEntityComponent* pProxy = GetProxy(proxy);
				if (pProxy)
				{
					if (pProxy->NeedGameSerialize())
					{
						bSaveProxies = true;
						break;
					}
				}
				proxyNrSerialized++;
			}
		}

		if (ser.BeginOptionalGroup("EntityProxies", bSaveProxies))
		{
			bool bHasSubst;
			if (!ser.IsReading())
				ser.Value("bHasSubst", bHasSubst = GetProxy(ENTITY_PROXY_SUBSTITUTION) != 0);
			else
			{
				ser.Value("bHasSubst", bHasSubst);
				if (bHasSubst && !GetProxy(ENTITY_PROXY_SUBSTITUTION))
					CreateProxy(ENTITY_PROXY_SUBSTITUTION);
			}

			//serializing the available proxies in the specified order (physics after script and user ..) [JAN]
			int proxyNrSerialized = 0;
			while (true)
			{
				EEntityProxy proxy = ProxySerializationOrder[proxyNrSerialized];

				if (proxy == ENTITY_PROXY_LAST)
					break;

				IEntityComponent* pProxy = GetProxy(proxy);
				if (pProxy)
				{
					pProxy->GameSerialize(ser);
				}

				proxyNrSerialized++;
			}
			assert(proxyNrSerialized == ENTITY_PROXY_LAST); //this checks whether every proxy is in the order list

			ser.EndGroup(); //EntityProxies
		}

		//////////////////////////////////////////////////////////////////////////
		// for usable on BasicEntity.
		//////////////////////////////////////////////////////////////////////////
		{
			IScriptTable* pScriptTable = GetScriptTable();
			if (pScriptTable)
			{
				if (ser.IsWriting())
				{
					if (pScriptTable->HaveValue("__usable"))
					{
						bool bUsable = false;
						pScriptTable->GetValue("__usable", bUsable);
						int nUsable = (bUsable) ? 1 : -1;
						ser.Value("__usable", nUsable);
					}
				}
				else
				{
					int nUsable = 0;
					ser.Value("__usable", nUsable);
					if (nUsable == 1)
						pScriptTable->SetValue("__usable", 1);
					else if (nUsable == -1)
						pScriptTable->SetValue("__usable", 0);
					else
						pScriptTable->SetToNull("__usable");
				}
			}
		}
	}

	if (nFlags & ENTITY_SERIALIZE_PROPERTIES)
	{
		CEntityComponentLuaScript* pScriptProxy = (CEntityComponentLuaScript*)GetProxy(ENTITY_PROXY_SCRIPT);
		if (pScriptProxy)
			pScriptProxy->SerializeProperties(ser);
	}

	m_bDirtyForwardDir = true;

}

//////////////////////////////////////////////////////////////////////////
void CEntity::Physicalize(SEntityPhysicalizeParams& params)
{
	m_physics.Physicalize(params);
}

void CEntity::AssignPhysicalEntity(IPhysicalEntity* pPhysEntity, int nSlot)
{
	m_physics.AssignPhysicalEntity(pPhysEntity, nSlot);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::EnablePhysics(bool enable)
{
	m_physics.EnablePhysics(enable);
}

bool CEntity::IsPhysicsEnabled() const
{
	return m_physics.IsPhysicsEnabled();
}

//////////////////////////////////////////////////////////////////////////
IPhysicalEntity* CEntity::GetPhysics() const
{
	IPhysicalEntity* pPhysicalEntity = m_physics.GetPhysicalEntity();
	if (pPhysicalEntity)
	{
		return pPhysicalEntity;
	}

	IEntityRopeComponent* pRopeProxy = (IEntityRopeComponent*)GetProxy(ENTITY_PROXY_ROPE);
	if (pRopeProxy)
	{
		IRopeRenderNode* pRopeRenderNode = pRopeProxy->GetRopeRenderNode();
		if (pRopeRenderNode)
		{
			return pRopeRenderNode->GetPhysics();
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// Custom entity material.
//////////////////////////////////////////////////////////////////////////
void CEntity::SetMaterial(IMaterial* pMaterial)
{
	m_pMaterial = pMaterial;

	m_render.UpdateRenderNodes();

	SEntityEvent event;
	event.event = ENTITY_EVENT_MATERIAL;
	event.nParam[0] = (INT_PTR)pMaterial;
	SendEvent(event);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetInHiddenLayer(bool bHiddenLayer)
{
	m_bIsInHiddenLayer = (bHiddenLayer ? 1 : 0);
	SEntityEvent e(bHiddenLayer ? ENTITY_EVENT_LAYER_HIDE : ENTITY_EVENT_LAYER_UNHIDE);
	SendEvent(e);
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CEntity::GetMaterial()
{
	return m_pMaterial;
}

//////////////////////////////////////////////////////////////////////////
int CEntity::PhysicalizeSlot(int slot, SEntityPhysicalizeParams& params)
{
	if (CEntityPhysics* proxy = GetPhysicalProxy())
	{
		return proxy->AddSlotGeometry(slot, params);
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UnphysicalizeSlot(int slot)
{
	if (CEntityPhysics* proxy = GetPhysicalProxy())
	{
		proxy->RemoveSlotGeometry(slot);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UpdateSlotPhysics(int slot)
{
	if (CEntityPhysics* proxy = GetPhysicalProxy())
	{
		proxy->UpdateSlotGeometry(slot, GetStatObj(slot));
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::IsSlotValid(int nSlot) const
{
	return m_render.IsSlotValid(nSlot);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::AllocateSlot()
{
	int nNewSlot = -1;
	m_render.GetOrMakeSlot(nNewSlot);
	return nNewSlot;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::FreeSlot(int nSlot)
{
	// Free any physical slot geometry that was added first, just in case
	m_physics.RemoveSlotGeometry(nSlot);
	// Now remove the render slot
	m_render.FreeSlot(nSlot);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::GetSlotCount() const
{
	return m_render.GetSlotCount();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::ClearSlots()
{
	m_render.FreeAllSlots();
}

//////////////////////////////////////////////////////////////////////////
CEntitySlot* CEntity::GetSlot(int nSlot) const
{
	return m_render.GetSlot(nSlot);
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::GetSlotInfo(int nSlot, SEntitySlotInfo& slotInfo) const
{
	return m_render.GetSlotInfo(nSlot, slotInfo);
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CEntity::GetSlotWorldTM(int nSlot) const
{
	return m_render.GetSlotWorldTM(nSlot);
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CEntity::GetSlotLocalTM(int nSlot, bool bRelativeToParent) const
{
	return m_render.GetSlotLocalTM(nSlot, bRelativeToParent);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetSlotLocalTM(int nSlot, const Matrix34& localTM, int nWhyFlags)
{
	Vec3 col0 = localTM.GetColumn(0);
	Vec3 col1 = localTM.GetColumn(1);
	Vec3 col2 = localTM.GetColumn(2);
	Vec3 col3 = localTM.GetColumn(3);

	CHECKQNAN_VEC(col0);
	CHECKQNAN_VEC(col1);
	CHECKQNAN_VEC(col2);
	CHECKQNAN_VEC(col3);

	m_render.SetSlotLocalTM(nSlot, localTM);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetSlotCameraSpacePos(int nSlot, const Vec3& cameraSpacePos)
{
	m_render.SetSlotCameraSpacePos(nSlot, cameraSpacePos);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GetSlotCameraSpacePos(int nSlot, Vec3& cameraSpacePos) const
{
	m_render.GetSlotCameraSpacePos(nSlot, cameraSpacePos);
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::SetParentSlot(int nParentSlot, int nChildSlot)
{
	return m_render.SetParentSlot(nParentSlot, nChildSlot);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetSlotMaterial(int nSlot, IMaterial* pMaterial)
{
	m_render.SetSlotMaterial(nSlot, pMaterial);
}

IMaterial* CEntity::GetSlotMaterial(int nSlot) const
{
	return m_render.GetSlotMaterial(nSlot);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetSlotFlags(int nSlot, uint32 nFlags)
{
	m_render.SetSlotFlags(nSlot, nFlags);
}

//////////////////////////////////////////////////////////////////////////
uint32 CEntity::GetSlotFlags(int nSlot) const
{
	return m_render.GetSlotFlags(nSlot);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::SetSlotRenderNode(int nSlot, IRenderNode* pRenderNode)
{
	return m_render.SetSlotRenderNode(nSlot, pRenderNode);
}

//////////////////////////////////////////////////////////////////////////
IRenderNode* CEntity::GetSlotRenderNode(int nSlot)
{
	CEntitySlot* pSlot = m_render.GetSlot(nSlot);
	if (pSlot)
	{
		return pSlot->GetRenderNode();
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UpdateSlotForComponent(IEntityComponent* pComponent)
{
	int slotId = pComponent->GetEntitySlotId();
	if (slotId == IEntityComponent::EmptySlotId)
	{
		slotId = AllocateSlot();
		pComponent->SetEntitySlotId(slotId);

	}
	if (pComponent->GetParent() != nullptr)
	{
		if (pComponent->GetParent()->GetEntitySlotId() == IEntityComponent::EmptySlotId)
		{
			UpdateSlotForComponent(pComponent->GetParent());
		}
		m_render.SetParentSlot(pComponent->GetParent()->GetEntitySlotId(), pComponent->GetEntitySlotId());
	}
	if (pComponent->GetTransform() != nullptr)
	{
		m_render.SetSlotLocalTM(slotId, pComponent->GetTransform()->ToMatrix34());
	}
	else
	{
		m_render.SetSlotLocalTM(slotId, Matrix34::CreateIdentity());
		pComponent->SetTransformMatrix(IDENTITY);
	}

	pComponent->OnTransformChanged();
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::ShouldUpdateCharacter(int nSlot) const
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
ICharacterInstance* CEntity::GetCharacter(int nSlot)
{
	return m_render.GetCharacter(nSlot);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::SetCharacter(ICharacterInstance* pCharacter, int nSlot, bool bUpdatePhysics)
{
	int nUsedSlot = -1;

	nUsedSlot = m_render.SetSlotCharacter(nSlot, pCharacter);
	if (bUpdatePhysics)
	{
		m_physics.UpdateSlotGeometry(nUsedSlot);
	}

	return nUsedSlot;
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CEntity::GetStatObj(int nSlot)
{
	return m_render.GetStatObj(nSlot);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::SetStatObj(IStatObj* pStatObj, int nSlot, bool bUpdatePhysics, float mass)
{
	int bNoSubslots = nSlot >= 0 || nSlot & ENTITY_SLOT_ACTUAL; // only use statobj's subslot when appending a new subslot
	nSlot = m_render.SetSlotGeometry(nSlot, pStatObj);
	if (bUpdatePhysics)
	{
		m_physics.UpdateSlotGeometry(nSlot, pStatObj, mass, bNoSubslots);
	}

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
IParticleEmitter* CEntity::GetParticleEmitter(int nSlot)
{
	return m_render.GetParticleEmitter(nSlot);
}

//////////////////////////////////////////////////////////////////////////
IGeomCacheRenderNode* CEntity::GetGeomCacheRenderNode(int nSlot)
{
#if defined(USE_GEOM_CACHES)
	if (m_render.IsSlotValid(nSlot))
	{
		return m_render.Slot(nSlot)->GetGeomCacheRenderNode();
	}
#endif

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
IRenderNode* CEntity::GetRenderNode(int nSlot) const
{
	return m_render.GetRenderNode(nSlot);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::MoveSlot(IEntity* targetIEnt, int nSlot)
{
	CEntity* targetEnt = (CEntity*)targetIEnt;

	CEntitySlot* srcSlot = GetSlot(nSlot);
	CEntitySlot* dstSlot = targetEnt->m_render.AllocSlot(nSlot);

	ICharacterInstance* pCharacterInstance = srcSlot->GetCharacter();

	//--- Shallow copy over slot
	dstSlot->ShallowCopyFrom(srcSlot);

	//--- Ensure that any associated physics is copied over too
	CEntityPhysics* srcPhysicsProxy = GetPhysicalProxy();
	if (pCharacterInstance)
	{
		ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
		IPhysicalEntity* pCharacterPhysics = pCharacterInstance->GetISkeletonPose()->GetCharacterPhysics();
		pCharacterInstance->GetISkeletonPose()->SetPostProcessCallback(NULL, NULL);
		if (srcPhysicsProxy && (pCharacterPhysics == srcPhysicsProxy->GetPhysicalEntity()))
		{
			srcPhysicsProxy->MovePhysics(targetEnt->GetPhysicalProxy());

			//Make sure auxiliary physics like ropes and attachments are also updated
			pe_params_foreign_data pfd;
			pfd.pForeignData = targetEnt;
			pfd.iForeignData = PHYS_FOREIGN_ID_ENTITY;

			for (int i = 0; pSkeletonPose->GetCharacterPhysics(i); ++i)
			{
				pSkeletonPose->GetCharacterPhysics(i)->SetParams(&pfd);
			}

			IAttachmentManager* pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
			const int attachmentCount = pAttachmentManager->GetAttachmentCount();
			for (int i = 0; i < attachmentCount; ++i)
			{
				IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(i);
				IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
				if ((pAttachment->GetType() == CA_BONE) && (pAttachmentObject != NULL))
				{
					ICharacterInstance* pAttachedCharacterInstance = pAttachmentObject->GetICharacterInstance();
					if (pAttachedCharacterInstance != NULL)
					{
						ISkeletonPose* pAttachedSkeletonPose = pAttachedCharacterInstance->GetISkeletonPose();
						IPhysicalEntity* pAttachedCharacterPhysics = pAttachedSkeletonPose->GetCharacterPhysics();
						if (pAttachedCharacterPhysics != NULL)
						{
							pAttachedCharacterPhysics->SetParams(&pfd);
						}

						for (int j = 0; pAttachedSkeletonPose->GetCharacterPhysics(j); ++j)
						{
							pAttachedSkeletonPose->GetCharacterPhysics(j)->SetParams(&pfd);
						}
					}
				}
			}

		}
		// Register ourselves to listen for animation events coming from the character.
		if (ISkeletonAnim* pSkeletonAnim = pCharacterInstance->GetISkeletonAnim())
			pSkeletonAnim->SetEventCallback(CEntityRender::AnimEventCallback, &targetEnt->m_render);
	}

	//--- Clear src slot
	srcSlot->Clear();

	InvalidateLocalBounds();
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadGeometry(int nSlot, const char* sFilename, const char* sGeomName, int nLoadFlags)
{
	nSlot = m_render.LoadGeometry(nSlot, sFilename, sGeomName, nLoadFlags);

	if (nLoadFlags & EF_AUTO_PHYSICALIZE)
	{
		// Also physicalize geometry.
		SEntityPhysicalizeParams params;
		params.nSlot = nSlot;
		IStatObj* pStatObj = GetStatObj(ENTITY_SLOT_ACTUAL | nSlot);
		float mass, density;
		if (!pStatObj || pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND ||
		    pStatObj->GetPhysicalProperties(mass, density) && (mass >= 0 || density >= 0))
			params.type = PE_RIGID;
		else
			params.type = PE_STATIC;
		Physicalize(params);

		IPhysicalEntity* pe = GetPhysics();
		if (pe)
		{
			pe_action_awake aa;
			aa.bAwake = 0;
			pe->Action(&aa);
			pe_params_foreign_data pfd;
			pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
			pe->SetParams(&pfd);
		}

		// Mark as AI hideable unless the object flags are preventing it.
		if (pStatObj && (pStatObj->GetFlags() & STATIC_OBJECT_NO_AUTO_HIDEPOINTS) == 0)
			AddFlags((uint32)ENTITY_FLAG_AI_HIDEABLE);
	}

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadCharacter(int nSlot, const char* sFilename, int nLoadFlags)
{
	LOADING_TIME_PROFILE_SECTION_ARGS(sFilename);

	ICharacterInstance* pChar;

	if ((pChar = m_render.GetCharacter(nSlot)) && GetPhysicalProxy() && pChar->GetISkeletonPose()->GetCharacterPhysics() == GetPhysics())
	{
		m_physics.AttachToPhysicalEntity(0);
	}

	nSlot = m_render.LoadCharacter(nSlot, sFilename, nLoadFlags);
	if (nLoadFlags & EF_AUTO_PHYSICALIZE)
	{
		ICharacterInstance* pCharacter = GetCharacter(nSlot);
		if (pCharacter)
			pCharacter->SetFlags(pCharacter->GetFlags() | CS_FLAG_UPDATE);

		// Also physicalize geometry.
		SEntityPhysicalizeParams params;
		params.nSlot = nSlot;
		params.type = PE_RIGID;
		Physicalize(params);

		IPhysicalEntity* pe = GetPhysics();
		if (pe)
		{
			pe_action_awake aa;
			aa.bAwake = 0;
			pe->Action(&aa);
			pe_params_foreign_data pfd;
			pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
			pe->SetParams(&pfd);
		}
	}
	return nSlot;
}

#if defined(USE_GEOM_CACHES)
int CEntity::LoadGeomCache(int nSlot, const char* sFilename)
{
	int ret = m_render.LoadGeomCache(nSlot, sFilename);

	SetMaterial(m_pMaterial);

	return ret;
}
#endif

//////////////////////////////////////////////////////////////////////////
int CEntity::SetParticleEmitter(int nSlot, IParticleEmitter* pEmitter, bool bSerialize)
{
	return m_render.SetParticleEmitter(nSlot, pEmitter, bSerialize);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadParticleEmitter(int nSlot, IParticleEffect* pEffect, SpawnParams const* params, bool bPrime, bool bSerialize)
{
	return m_render.LoadParticleEmitter(nSlot, pEffect, params, bPrime, bSerialize);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadLight(int nSlot, CDLight* pLight)
{
	return LoadLightImpl(nSlot, pLight);
}

int CEntity::LoadLightImpl(int nSlot, CDLight* pLight)
{
	uint16 layerId = ~0;
	return m_render.LoadLight(nSlot, pLight, layerId);
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::UpdateLightClipBounds(CDLight& light)
{
	bool bLightBoxValid = false;
	for (IEntityLink* pLink = m_pEntityLinks; pLink; pLink = pLink->next)
	{
		if (IEntity* pLinkedEntity = gEnv->pEntitySystem->GetEntity(pLink->entityId))
		{
			bool clipVolume = _stricmp(pLinkedEntity->GetClass()->GetName(), "ClipVolume") == 0;

			if (clipVolume)
			{
				if (IClipVolumeComponent* pVolume = static_cast<IClipVolumeComponent*>(pLinkedEntity->GetProxy(ENTITY_PROXY_CLIPVOLUME)))
				{
					for (int i = 0; i < 2; ++i)
					{
						if (light.m_pClipVolumes[i] == NULL)
						{
							light.m_pClipVolumes[i] = pVolume->GetClipVolume();
							light.m_Flags |= DLF_HAS_CLIP_VOLUME;

							break;
						}
					}
				}
			}
		}
	}

	return bLightBoxValid || (light.m_pClipVolumes[0] || light.m_pClipVolumes[1]);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadCloudBlocker(int nSlot, const SCloudBlockerProperties& properties)
{
	return m_render.LoadCloudBlocker(nSlot, properties);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::LoadFogVolume(int nSlot, const SFogVolumeProperties& properties)
{
	return m_render.LoadFogVolume(nSlot, properties);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::FadeGlobalDensity(int nSlot, float fadeTime, float newGlobalDensity)
{
	return m_render.FadeGlobalDensity(nSlot, fadeTime, newGlobalDensity);
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::RegisterInAISystem(const AIObjectParams& params)
{
	m_flags &= ~ENTITY_FLAG_HAS_AI;

	IAISystem* pAISystem = gEnv->pAISystem;
	if (pAISystem)
	{
		IAIObjectManager* pAIObjMgr = pAISystem->GetAIObjectManager();
		if (IAIObject* pAIObject = GetAIObject())
		{
			pAIObjMgr->RemoveObject(m_aiObjectID);
			m_aiObjectID = INVALID_AIOBJECTID;
			// The RemoveObject() call triggers immediate complete cleanup. Ideally the system would wait, as it does for internal removals. {2009/04/07}
		}

		if (params.type == 0)
			return true;

		AIObjectParams myparams(params);
		myparams.entityID = m_nID;
		myparams.name = GetName();

		if (IAIObject* pAIObject = pAIObjMgr->CreateAIObject(myparams))
		{
			m_aiObjectID = pAIObject->GetAIObjectID();

			m_flags |= ENTITY_FLAG_HAS_AI;

			UpdateAIObject();

			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// reflect changes in position or orientation to the AI object
void CEntity::UpdateAIObject()
{
	IAIObject* pAIObject = GetAIObject();
	if (pAIObject)
		pAIObject->SetPos(GetWorldPos(), GetForwardDir());
}

//////////////////////////////////////////////////////////////////////////
void CEntity::ActivateForNumUpdates(int numUpdates)
{
	IAIObject* pAIObject = GetAIObject();
	if (pAIObject && pAIObject->GetProxy())
		return;

	if (m_nUpdateCounter != 0)
	{
		m_nUpdateCounter = numUpdates;
		return;
	}

	m_nUpdateCounter = numUpdates;
	ActivateEntityIfNecessary();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetPhysicsState(XmlNodeRef& physicsState)
{
	if (physicsState)
	{
		IPhysicalEntity* physic = GetPhysics();
		if (!physic && GetCharacter(0))
			physic = GetCharacter(0)->GetISkeletonPose()->GetCharacterPhysics(0);
		if (physic)
		{
			IXmlSerializer* pSerializer = GetISystem()->GetXmlUtils()->CreateXmlSerializer();
			if (pSerializer)
			{
				ISerialize* ser = pSerializer->GetReader(physicsState);
				physic->SetStateFromSnapshot(ser);
				physic->PostSetStateFromSnapshot();
				pSerializer->Release();
				for (int i = GetSlotCount() - 1; i >= 0; i--)
					if (GetSlot(i)->GetCharacter())
						if (GetSlot(i)->GetCharacter()->GetISkeletonPose()->GetCharacterPhysics() == physic)
							GetSlot(i)->GetCharacter()->GetISkeletonPose()->SynchronizeWithPhysicalEntity(physic);
						else if (GetSlot(i)->GetCharacter()->GetISkeletonPose()->GetCharacterPhysics(0) == physic)
							GetSlot(i)->GetCharacter()->GetISkeletonPose()->SynchronizeWithPhysicalEntity(0, m_vPos, m_qRotation);
				ActivateForNumUpdates(5);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GetPhysicsWorldBounds(AABB& bounds) const
{
	m_physics.GetWorldBounds(bounds);
}

void CEntity::AddImpulse(int ipart, const Vec3& pos, const Vec3& impulse, bool bPos, float fAuxScale, float fPushScale)
{
	m_physics.AddImpulse(ipart, pos, impulse, bPos, fAuxScale, fPushScale);
}

bool CEntity::PhysicalizeFoliage(int iSlot)
{
	return m_physics.PhysicalizeFoliage(iSlot);
}

void CEntity::DephysicalizeFoliage(int iSlot)
{
	m_physics.DephysicalizeFoliage(iSlot);
}

//////////////////////////////////////////////////////////////////////////
int CEntity::GetPhysicalEntityPartId0(int islot)
{
	return m_physics.GetPartId0(islot);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::PhysicsNetSerializeEnable(bool enable)
{
	m_physics.EnableNetworkSerialization(enable);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::PhysicsNetSerializeTyped(TSerialize& ser, int type, int flags)
{
	m_physics.SerializeTyped(ser, type, flags);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::PhysicsNetSerialize(TSerialize& ser)
{
	m_physics.Serialize(ser);
}

//////////////////////////////////////////////////////////////////////////
IEntityLink* CEntity::GetEntityLinks()
{
	return m_pEntityLinks;
};

//////////////////////////////////////////////////////////////////////////
IEntityLink* CEntity::AddEntityLink(const char* sLinkName, EntityId entityId, EntityGUID entityGuid)
{
	assert(sLinkName);
	if (sLinkName == NULL)
		return NULL;

	IEntity* pLinkedEntity = GetEntitySystem()->GetEntity(entityId);

	IEntityLink* pNewLink = new IEntityLink;
	assert(strlen(sLinkName) <= ENTITY_LINK_NAME_MAX_LENGTH);
	cry_strcpy(pNewLink->name, sLinkName);
	pNewLink->entityId = entityId;
	pNewLink->entityGuid = entityGuid;
	pNewLink->next = 0;

	if (m_pEntityLinks)
	{
		IEntityLink* pLast = m_pEntityLinks;
		while (pLast->next)
			pLast = pLast->next;
		pLast->next = pNewLink;
	}
	else
	{
		m_pEntityLinks = pNewLink;
	}

	// Send event.
	SEntityEvent event(ENTITY_EVENT_LINK);
	event.nParam[0] = (INT_PTR)pNewLink;
	SendEvent(event);

	return pNewLink;
};

//////////////////////////////////////////////////////////////////////////
void CEntity::RenameEntityLink(IEntityLink* pLink, const char* sNewLinkName)
{
	if (!m_pEntityLinks || !pLink || !sNewLinkName)
		return;

	cry_strcpy(pLink->name, ENTITY_LINK_NAME_MAX_LENGTH, sNewLinkName);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::RemoveEntityLink(IEntityLink* pLink)
{
	if (!m_pEntityLinks || !pLink)
		return;

	// Send event.
	SEntityEvent event(ENTITY_EVENT_DELINK);
	event.nParam[0] = (INT_PTR)pLink;
	SendEvent(event);

	if (m_pEntityLinks == pLink)
	{
		m_pEntityLinks = m_pEntityLinks->next;
	}
	else
	{
		IEntityLink* pLast = m_pEntityLinks;
		while (pLast->next && pLast->next != pLink)
			pLast = pLast->next;
		if (pLast->next == pLink)
		{
			pLast->next = pLink->next;
		}
	}

	delete pLink;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::RemoveAllEntityLinks()
{
	IEntityLink* pLink = m_pEntityLinks;
	while (pLink)
	{
		m_pEntityLinks = pLink->next;

		// Send event.
		SEntityEvent event(ENTITY_EVENT_DELINK);
		event.nParam[0] = (INT_PTR)pLink;
		SendEvent(event);

		delete pLink;
		pLink = m_pEntityLinks;
	}
	m_pEntityLinks = 0;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	m_physics.GetMemoryUsage(pSizer);
	for (const SEntityComponentRecord& componentRecord : m_components.GetVector())
	{
		if (componentRecord.pComponent != nullptr)
		{
			componentRecord.pComponent->GetMemoryUsage(pSizer);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
#define DEF_ENTITY_EVENT_NAME(x) { x, # x }
struct SEventName
{
	EEntityEvent event;
	const char*  name;
} s_eventsName[] =
{
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_XFORM),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_TIMER),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_INIT),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DONE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_VISIBLITY),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_RESET),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ATTACH),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ATTACH_THIS),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DETACH),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DETACH_THIS),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LINK),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DELINK),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_HIDE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_UNHIDE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENABLE_PHYSICS),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PHYSICS_CHANGE_STATE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_SCRIPT_EVENT),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENTERAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LEAVEAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENTERNEARAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LEAVENEARAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_MOVEINSIDEAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_MOVENEARAREA),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PHYS_POSTSTEP),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PHYS_BREAK),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_AI_DONE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_AUDIO_TRIGGER_STARTED),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_AUDIO_TRIGGER_ENDED),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_COLLISION),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_RENDER_VISIBILITY_CHANGE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PREPHYSICSUPDATE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LEVEL_LOADED),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_START_LEVEL),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_START_GAME),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ENTER_SCRIPT_STATE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_LEAVE_SCRIPT_STATE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_PRE_SERIALIZE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_POST_SERIALIZE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_INVISIBLE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_VISIBLE),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_MATERIAL),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_ACTIVATED),
	DEF_ENTITY_EVENT_NAME(ENTITY_EVENT_DEACTIVATED),
};

//////////////////////////////////////////////////////////////////////////
void CEntity::LogEvent(SEntityEvent& event, CTimeValue dt)
{
	static int s_LastLoggedFrame = 0;

	int nFrameId = gEnv->pRenderer->GetFrameID(false);
	if (CVar::es_DebugEvents == 2 && s_LastLoggedFrame < nFrameId && s_LastLoggedFrame > nFrameId - 10)
	{
		// On Next frame turn it off.
		CVar::es_DebugEvents = 0;
		return;
	}

	// Find event, quite slow but ok for logging.
	char sNameId[32];
	const char* sName = "";
	for (int i = 0; i < CRY_ARRAY_COUNT(s_eventsName); i++)
	{
		if (s_eventsName[i].event == event.event)
		{
			sName = s_eventsName[i].name;
			break;
		}
	}
	if (!*sName)
	{
		cry_sprintf(sNameId, "%d", (int)event.event);
		sName = sNameId;
	}
	s_LastLoggedFrame = nFrameId;

	float fTimeMs = dt.GetMilliSeconds();
	CryLogAlways("<Frame:%d><EntityEvent> [%s](%X)\t[%.2fms]\t%s", nFrameId, sName, (int)event.nParam[0], fTimeMs, GetEntityTextDescription().c_str());
}

IAIObject* CEntity::GetAIObject()
{
	if (gEnv->pAISystem == nullptr)
		return nullptr;

	return (m_aiObjectID ? gEnv->pAISystem->GetAIObjectManager()->GetAIObject(m_aiObjectID) : nullptr);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::CreateSchematycObject(const SEntitySpawnParams& spawnParams)
{
	const CEntityClass* pClass = static_cast<const CEntityClass*>(spawnParams.pClass);

	Schematyc::IRuntimeClassConstPtr pRuntimeClass = pClass->GetSchematycRuntimeClass();
	if (pRuntimeClass)
	{
		if (!m_pSchematycProperties)
		{
			m_pSchematycProperties = pRuntimeClass->GetDefaultProperties().Clone();
		}
		if (spawnParams.entityNode)
		{
			XmlNodeRef schematycPropsNode = spawnParams.entityNode->findChild("SchematycProperties");
			if (schematycPropsNode)
			{
				// Load Schematyc object properties from the Entity Node XML data
				Serialization::LoadXmlNode(*m_pSchematycProperties.get(), schematycPropsNode);
			}
		}

		Schematyc::SObjectParams objectParams;
		objectParams.classGUID = pRuntimeClass->GetGUID();
		objectParams.pProperties = m_pSchematycProperties;
		objectParams.pEntity = this;

		if (gEnv->pSchematyc->CreateObject(objectParams, m_pSchematycObject))
		{
			if (m_simulationMode != EEntitySimulationMode::Idle)
			{
				m_pSchematycObject->SetSimulationMode(m_simulationMode, Schematyc::EObjectSimulationUpdatePolicy::OnChangeOnly, false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
Matrix34 CEntity::GetParentAttachPointWorldTM() const
{
	if (m_hierarchy.parentBindingType == EBindingType::eBT_GeomCacheNode)
	{
#if defined(USE_GEOM_CACHES)
		CGeomCacheAttachmentManager* pGeomCacheAttachmentManager = static_cast<CEntitySystem*>(GetEntitySystem())->GetGeomCacheAttachmentManager();
		return pGeomCacheAttachmentManager->GetNodeWorldTM(this, m_hierarchy.pParent);
#endif
	}
	else if (m_hierarchy.parentBindingType == EBindingType::eBT_CharacterBone)
	{
		CCharacterBoneAttachmentManager* pCharacterBoneAttachmentManager = static_cast<CEntitySystem*>(GetEntitySystem())->GetCharacterBoneAttachmentManager();
		return pCharacterBoneAttachmentManager->GetNodeWorldTM(this, m_hierarchy.pParent);
	}

	if (m_hierarchy.pParent)
		return m_hierarchy.pParent->m_worldTM;

	return Matrix34(IDENTITY);
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::IsParentAttachmentValid() const
{
	if (!m_hierarchy.pParent)
	{
		return false;
	}

	if (m_hierarchy.parentBindingType == EBindingType::eBT_GeomCacheNode)
	{
#if defined(USE_GEOM_CACHES)
		CGeomCacheAttachmentManager* pGeomCacheAttachmentManager = static_cast<CEntitySystem*>(GetEntitySystem())->GetGeomCacheAttachmentManager();
		return pGeomCacheAttachmentManager->IsAttachmentValid(this, m_hierarchy.pParent);
#endif
	}
	else if (m_hierarchy.parentBindingType == EBindingType::eBT_CharacterBone)
	{
		CCharacterBoneAttachmentManager* pCharacterBoneAttachmentManager = static_cast<CEntitySystem*>(GetEntitySystem())->GetCharacterBoneAttachmentManager();
		return pCharacterBoneAttachmentManager->IsAttachmentValid(this, m_hierarchy.pParent);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
IEntity* CEntity::GetAdam()
{
	for (IEntity* pAdam = GetParent(); pAdam; pAdam = pAdam->GetParent())
		if (!pAdam->GetParent() || ((CEntity*)pAdam)->m_hierarchy.parentBindingType == EBindingType::eBT_LocalSim)
			return pAdam;
	return this;
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::HandleVariableChange(const char* szVarName, const void* pVarData)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnRenderNodeVisibilityChange(bool bBecomeVisible)
{
	static bool bNoReentrant = false; // In case event callback do something with entity render nodes,to ignore reentrant callback from 3d engine
	if (bNoReentrant)
		return;

	bNoReentrant = true;
	if (GetFlags() & ENTITY_FLAG_SEND_RENDER_EVENT)
	{
		if (bBecomeVisible)
		{
			SEntityEvent entityEvent(ENTITY_EVENT_RENDER_VISIBILITY_CHANGE);
			entityEvent.nParam[0] = 1;
			SendEvent(entityEvent);
		}
		else
		{
			SEntityEvent entityEvent(ENTITY_EVENT_RENDER_VISIBILITY_CHANGE);
			entityEvent.nParam[0] = 0;
			SendEvent(entityEvent);
		}
	}
	bNoReentrant = false;
}

//////////////////////////////////////////////////////////////////////////
float CEntity::GetLastSeenTime() const
{
	return m_render.GetLastSeenTime();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::ComputeForwardDir() const
{
	if (m_bDirtyForwardDir)
	{
		if (IsScaled())
		{
			Matrix34 auxTM = m_worldTM;
			auxTM.OrthonormalizeFast();

			// assuming (0, 1, 0) as the local forward direction
			m_vForwardDir = auxTM.GetColumn1();
		}
		else
		{
			// assuming (0, 1, 0) as the local forward direction
			m_vForwardDir = m_worldTM.GetColumn1();
		}

		m_bDirtyForwardDir = false;
	}
}

INetEntity* CEntity::AssignNetEntityLegacy(INetEntity* ptr)
{
	auto ret = m_pNetEntity.get();
	m_pNetEntity.release();
	m_pNetEntity.reset(ptr);
	return ret;
}

INetEntity* CEntity::GetNetEntity()
{
	return m_pNetEntity.get();
}

uint32 CEntity::GetEditorObjectID() const
{
	return m_objectID >> 8;
}

void CEntity::SetObjectID(uint32 ID)
{
	m_objectID = (ID << 8) | (m_objectID & 0xFF);
}

void CEntity::GetEditorObjectInfo(bool& bSelected, bool& bHighlighted) const
{
	bSelected = (m_objectID & 1) != 0;
	bHighlighted = (m_objectID & (1 << 1)) != 0;
}

void CEntity::SetEditorObjectInfo(bool bSelected, bool bHighlighted)
{
	uint32 flags = (bSelected * (1)) | (bHighlighted * (1 << 1));
	m_objectID &= ~(0x3);
	m_objectID |= flags;

	int nSlots = GetSlotCount();

	for (int i = 0; i < nSlots; ++i)
	{
		IRenderNode* pRenderNode = GetRenderNode(i);
		if (pRenderNode)
		{
			pRenderNode->SetEditorObjectInfo(bSelected, bHighlighted);
		}
	}
}

void CEntity::ShutDownComponent(IEntityComponent* pComponent)
{
	if (pComponent)
	{
		pComponent->OnShutDown();
		// Free entity slot used by the component
		if (pComponent->GetEntitySlotId() != IEntityComponent::EmptySlotId)
		{
			pComponent->GetEntity()->FreeSlot(pComponent->GetEntitySlotId());
			pComponent->SetEntitySlotId(IEntityComponent::EmptySlotId);
		}
	}
}

void CEntityComponentsVector::ShutDownComponent(IEntityComponent* pComponent)
{
	CRY_ASSERT_MESSAGE(pComponent, "Invalid component pointer.");
	if (pComponent)
	{
		static_cast<CEntity*>(pComponent->GetEntity())->ShutDownComponent(pComponent);
	}
}