// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RuntimeAreaObject.h"

CRuntimeAreaObject::TAudioControlMap CRuntimeAreaObject::m_audioControls;

///////////////////////////////////////////////////////////////////////////
CRuntimeAreaObject::CRuntimeAreaObject()
{
}

///////////////////////////////////////////////////////////////////////////
CRuntimeAreaObject::~CRuntimeAreaObject()
{
}

//////////////////////////////////////////////////////////////////////////
bool CRuntimeAreaObject::Init(IGameObject* pGameObject)
{
	SetGameObject(pGameObject);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CRuntimeAreaObject::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	ResetGameObject();

	CRY_ASSERT_MESSAGE(false, "CRuntimeAreaObject::ReloadExtension not implemented");

	return false;
}

///////////////////////////////////////////////////////////////////////////
bool CRuntimeAreaObject::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	CRY_ASSERT_MESSAGE(false, "CRuntimeAreaObject::NetSerialize not implemented");

	return false;
}

///////////////////////////////////////////////////////////////////////////
void CRuntimeAreaObject::ProcessEvent(const SEntityEvent& entityEvent)
{
	switch (entityEvent.event)
	{
	case ENTITY_EVENT_ENTERAREA:
		{
			EntityId const nEntityID = static_cast<EntityId>(entityEvent.nParam[0]);

			IEntity* const pEntity = gEnv->pEntitySystem->GetEntity(nEntityID);
			if ((pEntity != NULL) && ((pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_CAN_COLLIDE_WITH_MERGED_MESHES) != 0))
			{
				TAudioParameterMap cNewEntityParamMap;

				UpdateParameterValues(pEntity, cNewEntityParamMap);

				m_activeEntitySounds.insert(
				  std::pair<EntityId, TAudioParameterMap>(static_cast<EntityId>(nEntityID), cNewEntityParamMap));
			}

			break;
		}
	case ENTITY_EVENT_LEAVEAREA:
		{
			EntityId const nEntityID = static_cast<EntityId>(entityEvent.nParam[0]);

			TEntitySoundsMap::iterator iFoundPair = m_activeEntitySounds.find(nEntityID);
			if (iFoundPair != m_activeEntitySounds.end())
			{
				StopEntitySounds(iFoundPair->first, iFoundPair->second);
				m_activeEntitySounds.erase(nEntityID);
			}

			break;
		}
	case ENTITY_EVENT_MOVEINSIDEAREA:
		{
			EntityId const nEntityID = static_cast<EntityId>(entityEvent.nParam[0]);
			TEntitySoundsMap::iterator iFoundPair = m_activeEntitySounds.find(nEntityID);

			if (iFoundPair != m_activeEntitySounds.end())
			{
				IEntity* const pEntity = gEnv->pEntitySystem->GetEntity(nEntityID);
				if ((pEntity != NULL) && ((pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_CAN_COLLIDE_WITH_MERGED_MESHES) != 0))
				{
					UpdateParameterValues(pEntity, iFoundPair->second);
				}
			}

			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////
uint64 CRuntimeAreaObject::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVEAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_MOVEINSIDEAREA);
}

///////////////////////////////////////////////////////////////////////////
void CRuntimeAreaObject::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}

void CRuntimeAreaObject::OnShutDown()
{
	// Stop all of the currently playing sounds controlled by this RuntimeAreaObject instance.
	for (TEntitySoundsMap::iterator iEntityData = m_activeEntitySounds.begin(),
		iEntityDataEnd = m_activeEntitySounds.end(); iEntityData != iEntityDataEnd; ++iEntityData)
	{
		StopEntitySounds(iEntityData->first, iEntityData->second);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRuntimeAreaObject::UpdateParameterValues(IEntity* const pEntity, TAudioParameterMap& paramMap)
{
	static float const fParamEpsilon = 0.001f;
	static float const fMaxDensity = 256.0f;

	IEntityAudioComponent* const pAudioProxy = pEntity->GetOrCreateComponent<IEntityAudioComponent>();
	if (pAudioProxy != NULL)
	{
		ISurfaceType* aSurfaceTypes[MMRM_MAX_SURFACE_TYPES];
		memset(aSurfaceTypes, 0x0, sizeof(aSurfaceTypes));

		float aDensities[MMRM_MAX_SURFACE_TYPES];
		memset(aDensities, 0x0, sizeof(aDensities));

		gEnv->p3DEngine->GetIMergedMeshesManager()->QueryDensity(pEntity->GetPos(), aSurfaceTypes, aDensities);

		for (int i = 0; i < MMRM_MAX_SURFACE_TYPES && (aSurfaceTypes[i] != NULL); ++i)
		{
			float const fNewParamValue = aDensities[i] / fMaxDensity;
			TSurfaceCRC const nSurfaceCrc = CCrc32::ComputeLowercase(aSurfaceTypes[i]->GetName());

			TAudioParameterMap::iterator iSoundPair = paramMap.find(nSurfaceCrc);
			if (iSoundPair == paramMap.end())
			{
				if (fNewParamValue > 0.0f)
				{
					// The sound for this surface is not yet playing on this entity, needs to be started.
					TAudioControlMap::const_iterator const iAudioControls = m_audioControls.find(nSurfaceCrc);
					if (iAudioControls != m_audioControls.end())
					{
						SAudioControls const& rAudioControls = iAudioControls->second;

						pAudioProxy->SetParameter(rAudioControls.audioRtpcId, fNewParamValue);
						pAudioProxy->ExecuteTrigger(rAudioControls.audioTriggerId);

						paramMap.insert(
						  std::pair<TSurfaceCRC, SAreaSoundInfo>(
						    nSurfaceCrc,
						    SAreaSoundInfo(rAudioControls, fNewParamValue)));
					}
				}
			}
			else
			{
				SAreaSoundInfo& oSoundInfo = iSoundPair->second;
				if (fabs_tpl(fNewParamValue - oSoundInfo.parameter) >= fParamEpsilon)
				{
					oSoundInfo.parameter = fNewParamValue;
					pAudioProxy->SetParameter(oSoundInfo.audioControls.audioRtpcId, oSoundInfo.parameter);
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CRuntimeAreaObject::StopEntitySounds(EntityId const entityId, TAudioParameterMap& paramMap)
{
	IEntity* const pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	if (pEntity != NULL)
	{
		IEntityAudioComponent* const pAudioProxy = pEntity->GetOrCreateComponent<IEntityAudioComponent>();
		if (pAudioProxy != NULL)
		{
			for (TAudioParameterMap::const_iterator iSoundPair = paramMap.begin(), iSoundPairEnd = paramMap.end(); iSoundPair != iSoundPairEnd; ++iSoundPair)
			{
				pAudioProxy->StopTrigger(iSoundPair->second.audioControls.audioTriggerId);
				pAudioProxy->SetParameter(iSoundPair->second.audioControls.audioRtpcId, 0.0f);
			}

			paramMap.clear();
		}
	}
}
