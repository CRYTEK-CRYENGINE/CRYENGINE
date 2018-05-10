// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.


#include "StdAfx.h"

#include "CryMacros.h"
#include "PlayerVisTable.h"

#include "Game.h"
#include "GameCVars.h"
#include "Player.h"
#include "SmokeManager.h"
#include "Utility/CryWatch.h"
#include "EnvironmentalWeapon.h"

#define PIERCE_GLASS (13)

CPlayerVisTable::CPlayerVisTable()
	: m_numUsedVisTableEntries(0)
	, m_numLinetestsThisFrame(0)
	, m_currentNumIgnoreEntities(0)
	, m_currentBufferTarget(0)
	, m_currentBufferProcessing(1)
#if ALLOW_VISTABLE_DEBUGGING
	, m_numQueriesThisFrame(0)
#endif
{
	memset(m_visTablePriorities, 0, sizeof(m_visTablePriorities));
	ClearGlobalIgnoreEntities(); 
}

CPlayerVisTable::~CPlayerVisTable()
{
}

bool CPlayerVisTable::CanLocalPlayerSee(const SVisibilityParams& target, uint8 acceptableFrameLatency)
{
	assert(acceptableFrameLatency < 128); //If this is too high there is the potential for the framesSinceLastCheck to overflow

	bool localPlayerCanSee = false;

	CryPrefetch(m_visTableEntries);

	VisEntryIndex visIndex = GetEntityIndexFromID(target.targetEntityId);

#if ALLOW_VISTABLE_DEBUGGING
	m_numQueriesThisFrame++;
#endif


	if(visIndex != -1)
	{
		SVisTableEntry& visInfo = m_visTableEntries[visIndex];

		visInfo.lastRequestedLatency = ((visInfo.lastRequestedLatency + acceptableFrameLatency + 1) / 2);
		visInfo.flags |= eVF_Requested;
		if (target.queryParams & eVQP_UseCenterAsReference)
		{
			visInfo.flags |= eVF_CheckAgainstCenter;
		}
		visInfo.heightOffset   = target.heightOffset;

		localPlayerCanSee = (target.queryParams & eVQP_IgnoreGlass) != 0 ? (visInfo.flags & eVF_VisibleThroughGlass) != 0 : (visInfo.flags & eVF_Visible) != 0;
	
		const bool checkAgainstSmoke = localPlayerCanSee && !(target.queryParams&eVQP_IgnoreSmoke);
		if(checkAgainstSmoke)
		{
			//Now check vs obscuring smoke grenades. These are created at runtime, randomly,
			//	and have a short lifetime. Timeslicing the checks against them could render them
			//	largely ineffective - Rich S

			Vec3 localPlayerPosn;

			GetLocalPlayerPosn(localPlayerPosn);

			IEntity * pEntity = gEnv->pEntitySystem->GetEntity(visInfo.entityId);

			if(pEntity)
			{
				if(!CSmokeManager::GetSmokeManager()->CanSeePointThroughSmoke(pEntity->GetPos(), localPlayerPosn))
				{
					localPlayerCanSee = false;
					visInfo.flags &= ~(eVF_Visible | eVF_VisibleThroughGlass);
				}
			}			
		}
	}
	else
	{
		SVisTableEntry& visInfo = m_visTableEntries[m_numUsedVisTableEntries];

		visInfo.flags = eVF_Requested;
		if (target.queryParams & eVQP_UseCenterAsReference)
		{
			visInfo.flags |= eVF_CheckAgainstCenter;
		}
		visInfo.entityId = target.targetEntityId;
		visInfo.lastRequestedLatency = acceptableFrameLatency;
		visInfo.framesSinceLastCheck = acceptableFrameLatency;
		visInfo.heightOffset = target.heightOffset;

		localPlayerCanSee = false;

		m_numUsedVisTableEntries++;

		assert(m_numUsedVisTableEntries <= kMaxVisTableEntries);
	}

	return localPlayerCanSee;
}

bool CPlayerVisTable::CanLocalPlayerSee(const SVisibilityParams& target)
{
	return CanLocalPlayerSee(target, kDefaultAcceptableLatency);
}


void CPlayerVisTable::RemovePendingDeferredLinetest(const VisEntryIndex visEntryIndex)
{
	for(TLinetestIndex linetestIndex = 0; linetestIndex < kNumVisTableBuffers; linetestIndex++)
	{
		SDeferredLinetestReceiver * pendingEntry = GetDeferredLinetestReceiverFromVisTableIndex(visEntryIndex, linetestIndex);
		
		if(pendingEntry)
		{
			if(pendingEntry->IsValid())
			{
				SDeferredLinetestBuffer& visBuffer = m_linetestBuffers[linetestIndex];
				visBuffer.m_numLinetestsCurrentlyProcessing--;
				pendingEntry->SetInvalid(); // cancels the query
				pendingEntry->SetFree();
			}
		}		
	}	
}

void CPlayerVisTable::UpdatePendingDeferredLinetest(const VisEntryIndex source, const VisEntryIndex dest)
{
	for(TLinetestIndex linetestIndex = 0; linetestIndex < kNumVisTableBuffers; linetestIndex++)
	{
		SDeferredLinetestReceiver * pendingEntry = GetDeferredLinetestReceiverFromVisTableIndex(source, linetestIndex);

		if(pendingEntry)
		{
			pendingEntry->visTableIndex = dest;
		}		
	}
}

SDeferredLinetestReceiver * CPlayerVisTable::GetDeferredLinetestReceiverFromVisTableIndex(const VisEntryIndex visEntryIndex, const TLinetestIndex linetestIndex)
{
	SDeferredLinetestBuffer& visBuffer = m_linetestBuffers[linetestIndex];

	SDeferredLinetestReceiver * visTableProcessingEntries = visBuffer.m_deferredLinetestReceivers;

	VisEntryIndex processingIndex = -1;

	for(int receiverIndex = 0; receiverIndex < kMaxVisTableLinetestsPerFrame; receiverIndex++)
	{
		if(visTableProcessingEntries[receiverIndex].visTableIndex == visEntryIndex)
		{
			return &visBuffer.m_deferredLinetestReceivers[receiverIndex];
		}
	}

	return NULL;
}

void CPlayerVisTable::RemoveNthEntity(const VisEntryIndex n)
{
	CRY_ASSERT(m_numUsedVisTableEntries > 0);

	//NOTE: This does not have to be synched as the access will be on this thread.

	const VisEntryIndex swapIndex = m_numUsedVisTableEntries - 1;

	if(m_visTableEntries[n].flags & eVF_Pending)
	{
		RemovePendingDeferredLinetest(n);
	}

	if(m_visTableEntries[swapIndex].flags & eVF_Pending && n != swapIndex)
	{
		UpdatePendingDeferredLinetest(swapIndex, n);
	}

	m_visTableEntries[n] = m_visTableEntries[swapIndex];
	m_visTableEntries[swapIndex].Reset();
	m_numUsedVisTableEntries--;
}

void CPlayerVisTable::Update(float dt)
{
	const int numEntries = m_numUsedVisTableEntries;

	CryPrefetch(m_visTableEntries);

	//Flip the buffers
	m_currentBufferProcessing = 1 - m_currentBufferProcessing;
	m_currentBufferTarget			= 1 - m_currentBufferTarget;

	Vec3 localPlayerPosn;

	GetLocalPlayerPosn(localPlayerPosn);

	//Iterate over all of the vis table entries and calculate the priorities

	int numAdded = AddVisTableEntriesToPriorityList();

	//Iterate over all of the vis table entries
	for(int i = 0; i < numAdded; ++i)
	{
		const SVisTablePriority& visTableEntry = m_visTablePriorities[i];
		DoVisibilityCheck(localPlayerPosn, *visTableEntry.visInfo, visTableEntry.visIndex);
	}

	const int kNumVistableEntries = m_numUsedVisTableEntries;
	for(int i = 0; i < kNumVistableEntries; i++)
	{
		m_visTableEntries[i].framesSinceLastCheck++;			
	}

#if ALLOW_VISTABLE_DEBUGGING
	if(g_pGameCVars->pl_debug_vistableIgnoreEntities)
	{
		UpdateIgnoreEntityDebug(); 
	}

	if(g_pGameCVars->pl_debug_vistable)
	{
		const float white[] = {1.0f,1.0f,1.0f,1.0f};

		const int kNumUsedVistableEntries = m_numUsedVisTableEntries;
		VisEntryIndex worstIndex = -1;

		int worstLatency = 0;
		for(int i = 0; i < kNumUsedVistableEntries; i++)
		{
			if((m_visTableEntries[i].flags & eVF_Requested) && !(m_visTableEntries[i].flags & eVF_Remove) && (m_visTableEntries[i].framesSinceLastCheck > worstLatency))
			{
				worstLatency	= m_visTableEntries[i].framesSinceLastCheck;
				worstIndex		= i;
			}
		}

		int assertAfterThisManyFrames = g_pGameCVars->g_assertWhenVisTableNotUpdatedForNumFrames;
		if (worstLatency >= assertAfterThisManyFrames && worstIndex > 0 && worstIndex < kNumUsedVistableEntries)
		{
			IEntity * entity = gEnv->pEntitySystem->GetEntity(m_visTableEntries[worstIndex].entityId);
			CRY_ASSERT_MESSAGE(false, string().Format("%u frames have passed since last check of vis-table element %d (entity %d = %s \"%s\") flags=%u", m_visTableEntries[worstIndex].framesSinceLastCheck, worstIndex, m_visTableEntries[worstIndex].entityId, entity ? entity->GetClass()->GetName() : "NULL", entity ? entity->GetName() : "NULL", m_visTableEntries[worstIndex].flags));
		}


		IRenderAuxText::Draw2dLabel(20.f, 500.f, 1.5f, white, false, "VistableInfo:\n  Num Linetests this frame: %d\n  Num queries this frame: %d\n  Worst latency: %d", m_numLinetestsThisFrame, m_numQueriesThisFrame, worstLatency);
	
		if (g_pGameCVars->pl_debug_vistable == 2)
		{
			m_debugDraw.Update();
		}
	}

	m_numQueriesThisFrame = 0;
#endif

	m_numLinetestsThisFrame = 0;

	ClearRemovedEntities();
}

#if ALLOW_VISTABLE_DEBUGGING
void CPlayerVisTable::UpdateIgnoreEntityDebug()
{
	// Output array contents
	CryWatch("==== VISTABLE: Ignore Entities ====");
	CryWatch(""); 
	const int finalIndex = m_currentNumIgnoreEntities; 
	for(int i = 0; i <= finalIndex; ++i)
	{
		const char* pEntName = "Unknown";
		IEntity* pIgnoreEntity = gEnv->pEntitySystem->GetEntity(m_globalIgnoreEntities[i].id);
		if(pIgnoreEntity)
		{
			pEntName = pIgnoreEntity->GetName(); 
		}

		CryWatch("Entity: Id < %d > , Name < %s >", m_globalIgnoreEntities[i].id, pEntName);
		CryWatch("RefCount %d", m_globalIgnoreEntities[i].refCount);

		if(m_globalIgnoreEntities[i].requesteeName.c_str())
		{
			CryWatch("Caller %s", m_globalIgnoreEntities[i].requesteeName.c_str());
		}

		CryWatch(""); 
	}
	CryWatch("==== ==== ==== ==== ==== ==== ====");

	// Test adding / removing specified ids
	if(g_pGameCVars->pl_debug_vistableAddEntityId)
	{
		AddGlobalIgnoreEntity(g_pGameCVars->pl_debug_vistableAddEntityId, "VisTable DEBUG");
		g_pGameCVars->pl_debug_vistableAddEntityId = 0; 
	}

	if(g_pGameCVars->pl_debug_vistableRemoveEntityId)
	{
		RemoveGlobalIgnoreEntity(g_pGameCVars->pl_debug_vistableRemoveEntityId);
		g_pGameCVars->pl_debug_vistableRemoveEntityId = 0; 
	}
}
#endif // #if ALLOW_VISTABLE_DEBUGGING


//---------------------------------------------
// CPlayerVisTable::AddVisTableEntriesToPriorityList()
//		Scans the current list of entities in the vis table and looks
//		for the highest priority entities to test to. Any entities
//		that have not been involved in a request are marked for
//		removal from the vistable

int CPlayerVisTable::AddVisTableEntriesToPriorityList()
{
	int numAdded = 0;
	int worstPriority = std::numeric_limits<int>::min();
	int worstPriorityIndex = 0;

	const int kNumUsedVisTableEntries = m_numUsedVisTableEntries;

	for(int i = 0; i < kNumUsedVisTableEntries; i++)
	{
		CryPrefetch(&m_visTableEntries[i+4]);

		SVisTableEntry& visInfo = m_visTableEntries[i];

		const int lastRequestedLatency = visInfo.lastRequestedLatency;
		const int framesSinceLastCheck = visInfo.framesSinceLastCheck;
		int newEntryPriority = lastRequestedLatency - framesSinceLastCheck;

		if ((visInfo.flags & eVF_Requested) && !(visInfo.flags & eVF_Pending))
		{
			if(numAdded < kMaxVisTableLinetestsPerFrame)
			{	
				SVisTablePriority& visPriority = m_visTablePriorities[numAdded];
				visPriority.visInfo = &visInfo;
				visPriority.priority = newEntryPriority;
				visPriority.visIndex = i;

				if(newEntryPriority > worstPriority)
				{
					worstPriority				= newEntryPriority;
					worstPriorityIndex	= numAdded;					
				}

				numAdded++;
			}
			else if(newEntryPriority < worstPriority)
			{
				SVisTablePriority& worstVisPriority = m_visTablePriorities[worstPriorityIndex];

				worstPriority = newEntryPriority;
				worstVisPriority.priority		= newEntryPriority;
				worstVisPriority.visInfo		= &visInfo;
				worstVisPriority.visIndex		= i;

				for(int j = 0; j < numAdded; j++)
				{
					SVisTablePriority& visP = m_visTablePriorities[j];
					if(worstPriority <= visP.priority )
					{
						worstPriority				= visP.priority;
						worstPriorityIndex	= j;
					}
				}
			}
		}
		else
		{
			if(visInfo.framesSinceLastCheck > (kMinUnusedFramesBeforeEntryRemoved + visInfo.lastRequestedLatency))
			{
				visInfo.flags |= eVF_Remove;
			}
		}
	}

	return numAdded;
}

SDeferredLinetestReceiver * CPlayerVisTable::GetAvailableDeferredLinetestReceiver(SDeferredLinetestBuffer& visBuffer)
{
	for(int i = 0; i < kMaxVisTableLinetestsPerFrame; i++)
	{
		if(visBuffer.m_deferredLinetestReceivers[i].IsFree())
		{
			return &visBuffer.m_deferredLinetestReceivers[i];
		}
	}

	assert(!"Failed to find free processing entry when one was present according to m_numLinetestsCurrentlyProcessing");
	CryLogAlways("[RS] GetAvailableEntryForProcessing: Failed to find a valid index, about to memory overwrite\n[RS]  Num Entries Processing: %d\n[RS]  Num Free: %d\n", visBuffer.m_numLinetestsCurrentlyProcessing, kMaxVisTableLinetestsPerFrame - visBuffer.m_numLinetestsCurrentlyProcessing);

	return NULL;
}

//---------------------------------------------
// Check to see if the local player can see a given entity.
//		This function can flag entities as visible or not,
//		or to be removed.

void CPlayerVisTable::DoVisibilityCheck(const Vec3& localPlayerPosn, SVisTableEntry& visInfo, VisEntryIndex visIndex)
{
	Vec3 vecToTarget;

	IEntity * pEntity = gEnv->pEntitySystem->GetEntity(visInfo.entityId);

	if(pEntity)
	{
		IPhysicalEntity* pTargetPhysEnt = pEntity->GetPhysics();

		SDeferredLinetestBuffer& targetBuffer = m_linetestBuffers[GetCurrentLinetestBufferTargetIndex()];

		if(targetBuffer.m_numLinetestsCurrentlyProcessing < kMaxVisTableLinetestsPerFrame)
		{
			visInfo.framesSinceLastCheck = 0;

			SDeferredLinetestReceiver * processingEntry = GetAvailableDeferredLinetestReceiver(targetBuffer);

			assert(processingEntry);

			Vec3 targetPosn = pEntity->GetWorldPos();
			if (visInfo.flags & eVF_CheckAgainstCenter)
			{
				AABB targetBbox;
				pEntity->GetWorldBounds(targetBbox);
				if (!targetBbox.IsEmpty())
				{
					targetPosn = targetBbox.GetCenter();
				}
				float radius = min(min(targetBbox.max.x-targetBbox.min.x, targetBbox.max.y-targetBbox.min.y), targetBbox.max.z-targetBbox.min.z);
				targetPosn += (localPlayerPosn-targetPosn).GetNormalized() * radius * 0.25f;
			}
			targetPosn.z += visInfo.heightOffset;
			vecToTarget = targetPosn - localPlayerPosn;

			processingEntry->visTableIndex = visIndex;			

			ray_hit hit;
			const int rayFlags = rwi_colltype_any(geom_colltype_solid&(~geom_colltype_player)) | rwi_ignore_noncolliding | rwi_pierceability(PIERCE_GLASS);

			m_numLinetestsThisFrame++;
			targetBuffer.m_numLinetestsCurrentlyProcessing++;

			visInfo.flags |= eVF_Pending;
			visInfo.flags &= ~eVF_CheckAgainstCenter;

			processingEntry->visBufferIndex = m_currentBufferTarget;

			const int numEntries = kMaxNumIgnoreEntities + 1; 
			IPhysicalEntity* pSkipEnts[numEntries];
			int numSkipEnts = 0;

			if(pTargetPhysEnt)
			{
				pSkipEnts[numSkipEnts] = pTargetPhysEnt;
				numSkipEnts++;
			}
			if (m_currentNumIgnoreEntities)
			{
				for(int i = 0; i < m_currentNumIgnoreEntities; ++i)
				{
					SIgnoreEntity& ignoreEnt = m_globalIgnoreEntities[i];
					CRY_ASSERT(ignoreEnt.id); 

					IEntity* pIgnoreEntity = gEnv->pEntitySystem->GetEntity(ignoreEnt.id);
					IPhysicalEntity* pIgnorePhysicsEntity = pIgnoreEntity ? pIgnoreEntity->GetPhysics() : NULL;

					if (pIgnorePhysicsEntity)
					{
						pSkipEnts[numSkipEnts] = pIgnorePhysicsEntity;
						numSkipEnts++;
					}
				}
			}
			
			CRY_ASSERT(processingEntry->queuedRayID == 0);

			processingEntry->queuedRayID = g_pGame->GetRayCaster().Queue(
				RayCastRequest::HighPriority,
				RayCastRequest(localPlayerPosn, vecToTarget,
				ent_terrain|ent_static|ent_sleeping_rigid|ent_rigid,
				rayFlags,
				pSkipEnts,
				numSkipEnts, 2),
				functor(*processingEntry, &SDeferredLinetestReceiver::OnDataReceived));

#if ALLOW_VISTABLE_DEBUGGING
			m_debugDraw.UpdateDebugTarget(visInfo.entityId, targetPosn, ((visInfo.flags & eVF_Visible) != 0));
#endif

		}
	}
	else
	{
		visInfo.flags |= eVF_Remove;
	}
}


//---------------------------------------------
// Fills out the local player position

void CPlayerVisTable::GetLocalPlayerPosn(Vec3& localPlayerPosn)
{
	const CCamera& camera = gEnv->pSystem->GetViewCamera();
	localPlayerPosn = camera.GetPosition();
}

//---------------------------------------------
// Actually removes any entites from the list that have been flagged

void CPlayerVisTable::ClearRemovedEntities()
{
	for(VisEntryIndex i = 0; i < m_numUsedVisTableEntries; i++)
	{
		CRY_TODO(30,09,2009, "Prefetch to avoid cache misses");

		SVisTableEntry& visInfo = m_visTableEntries[i];

		if(visInfo.flags & eVF_Remove)
		{
			RemoveNthEntity(i);
		}
	}
}

VisEntryIndex CPlayerVisTable::GetEntityIndexFromID(EntityId entityId)
{
	CRY_TODO(30,09,2009, "Make faster");
	CryPrefetch(((char*)m_visTableEntries) + 128);

	for(VisEntryIndex i = 0; i < m_numUsedVisTableEntries; i++)
	{
		SVisTableEntry& visInfo = m_visTableEntries[i];

		if(m_visTableEntries[i].entityId == entityId)
		{
			return i;
		}		
	}

	return -1;
}

void CPlayerVisTable::Reset()
{
	//Note: This isn't curently being called and is provided for completeness.
	//		  Due to the threading of deferred linetests there may (and probably will)
	//			be issues if this is called with linetests currently outstanding

	m_numUsedVisTableEntries = 0;

	for(int i = 0; i < kNumVisTableBuffers; i++)
	{
		SDeferredLinetestBuffer& visBuffer = m_linetestBuffers[i];

		visBuffer.m_numLinetestsCurrentlyProcessing = 0;		

		for(int j = 0; j < kMaxVisTableLinetestsPerFrame; j++)
		{
			visBuffer.m_deferredLinetestReceivers[j].SetInvalid();
			visBuffer.m_deferredLinetestReceivers[j].SetFree();			
		}
	}

	ClearGlobalIgnoreEntities(); 
}

void CPlayerVisTable::AddGlobalIgnoreEntity( const EntityId entId ,const char* pCallerName )
{
	// For now (whilst small capacity only.. and infrequent access) we do O(N) search. Can revise this if necessary
	for(int i = 0; i < kMaxNumIgnoreEntities; ++i)
	{
		SIgnoreEntity& ignoreEnt = m_globalIgnoreEntities[i];
		if(ignoreEnt.id == entId)
		{
			++(ignoreEnt.refCount); 
			return;
		}

		if(!ignoreEnt.id)
		{
			ignoreEnt.id	   = entId;
			ignoreEnt.refCount = 1; 

#if ALLOW_VISTABLE_DEBUGGING
			if(pCallerName)
			{
				ignoreEnt.requesteeName.Format("Caller: %s", pCallerName);
			}
			else
			{
				ignoreEnt.requesteeName = "Unknown";
			}
#endif 

			++m_currentNumIgnoreEntities;
			return; 
		}
	}

	// should never really reach here else require increased buffer size (could assert if we feel more important)
	CryLogAlways("CPlayerVisTable::AddGlobalIgnoreEntity( const EntityId entId ) < WARNING - exceeded kMaxNumIgnoreEntities, should increase buffer size"); 
	
}

void CPlayerVisTable::RemoveGlobalIgnoreEntity( const EntityId entId )
{

	if(m_currentNumIgnoreEntities == 0 || entId == 0)
	{
		CryLogAlways("CPlayerVisTable::AddGlobalIgnoreEntity( const EntityId entId ) < WARNING - attempting to remove ignore entity that is not present - no current ignore entities set"); 
		return;
	}

	CRY_ASSERT(m_currentNumIgnoreEntities <= kMaxNumIgnoreEntities); 

	// For now (whilst small capacity only.. and infrequent access) we do O(N) search. Can revise this if necessary(e.g. sorted by ent id + binary search.. but not worthwhile with 8 capacity)
	const int finalIndex = m_currentNumIgnoreEntities; 
	for(int i = 0; i < m_currentNumIgnoreEntities; ++i)
	{
		SIgnoreEntity& ignoreEnt = m_globalIgnoreEntities[i];
		if(ignoreEnt.id == entId)
		{
			--ignoreEnt.refCount;
			if(ignoreEnt.refCount <= 0)
			{
				if(i < finalIndex) 
				{
					// If not the last entry, overwrite with last valid in array
					m_globalIgnoreEntities[i] = m_globalIgnoreEntities[finalIndex];
					m_globalIgnoreEntities[finalIndex].Clear(); 
				}
				else
				{
					CRY_ASSERT(i == finalIndex);
					m_globalIgnoreEntities[i].Clear(); 
				}
				--m_currentNumIgnoreEntities;
			}
			return;
		}

		if(i == finalIndex)
		{
			// Its not here
			CryLogAlways("CPlayerVisTable::RemoveGlobalIgnoreEntity( const EntityId entId ) < WARNING - attempting to remove ignore entity that is not present"); 
			return; 
		}
	}

}

void CPlayerVisTable::ClearGlobalIgnoreEntities()
{
	memset(m_globalIgnoreEntities, 0, kMaxNumIgnoreEntities * sizeof(SIgnoreEntity)); 
	m_currentNumIgnoreEntities = 0; 
}

void SDeferredLinetestReceiver::OnDataReceived( const QueuedRayID& rayID, const RayCastResult& result )
{
	CRY_ASSERT(rayID == queuedRayID);

	CPlayerVisTable * visTable = g_pGame->GetPlayerVisTable();

	//This may be receiving results from the previous frame's physics, when
	//	 the game has just been reset and the vis table with it
	if(!IsFree())
	{
		SDeferredLinetestBuffer& visBuffer = visTable->GetDeferredLinetestBuffer(visBufferIndex);

		if(IsValid())
		{
			SVisTableEntry& visEntry = visTable->GetNthVisTableEntry(visTableIndex);
			const bool obstructed = (result.hitCount > 0);

			if(obstructed)
			{
				//The linetest hit something en route to the target
				visEntry.flags &= ~(eVF_Visible|eVF_VisibleThroughGlass|eVF_Requested|eVF_Pending|eVF_CheckAgainstCenter);

				bool isGlass = true;
				int i = 0;
				
				while(i < result.hitCount && isGlass)
				{
					float bouncy, friction;
					uint32	pierceabilityMat;
					gEnv->pPhysicalWorld->GetSurfaceParameters(result.hits[i].surface_idx, bouncy, friction, pierceabilityMat);
					pierceabilityMat &= sf_pierceable_mask;

					if(pierceabilityMat <= PIERCE_GLASS)
					{
						isGlass = false;
					}

					i++;
				}
				
				if(isGlass)
				{
					visEntry.flags |= eVF_VisibleThroughGlass;
				}
				else if (result->pCollider->GetType() == PE_ARTICULATED)
				{
					visEntry.flags |= eVF_Visible;
				}
				else if(IEntity* pTarget = gEnv->pEntitySystem->GetEntityFromPhysics(result->pCollider))
				{
					// Test if this is an environmental weapon. If so, get its OWNER (the owner is still visible). 
					CEnvironmentalWeapon *pEnvWeapon = static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(pTarget->GetId(), "EnvironmentalWeapon"));
					if(pEnvWeapon && pEnvWeapon->GetOwner() == visEntry.entityId)
					{
						visEntry.flags |= eVF_Visible | eVF_VisibleThroughGlass;
					}
				}
			}
			else
			{
				visEntry.flags |= eVF_Visible | eVF_VisibleThroughGlass;
				visEntry.flags &= ~(eVF_Requested|eVF_Pending|eVF_CheckAgainstCenter);
			}

#if ALLOW_VISTABLE_DEBUGGING
			visTable->GetDebugDraw().UpdateDebugTarget(visEntry.entityId, ZERO, !obstructed);
#endif
			queuedRayID = 0;	//Pending ray processed

			SetInvalid();
			visEntry.framesSinceLastCheck = 0;
		}

		//Mark this SVisTableEntry_Processing as available
		SetFree();
		visBuffer.m_numLinetestsCurrentlyProcessing--;
	}
}

void SDeferredLinetestReceiver::SetInvalid()
{
	visTableIndex = -1;
	CancelPendingRay();
}

void SDeferredLinetestReceiver::CancelPendingRay()
{
	if (queuedRayID != 0)
	{
		g_pGame->GetRayCaster().Cancel(queuedRayID);
	}
	queuedRayID = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if ALLOW_VISTABLE_DEBUGGING
CPlayerVisTableDebugDraw::CPlayerVisTableDebugDraw()
{
	m_debugTargets.reserve(32);
}

void CPlayerVisTableDebugDraw::UpdateDebugTarget( const EntityId entityId, const Vec3& worldRefPoint, bool visible )
{
	if (g_pGameCVars->pl_debug_vistable != 2)
		return;

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);

	if (pEntity)
	{
		SDebugInfo* pTargetInfo = GetDebugInfoForEntity(entityId);
		const bool updateRefPoint = !worldRefPoint.IsZero();

		if (pTargetInfo)
		{
			pTargetInfo->m_visible = visible;
			pTargetInfo->m_lastUpdatedTime = gEnv->pTimer->GetCurrTime();
			if (updateRefPoint)
			{
				pTargetInfo->m_localTargetPos = pEntity->GetWorldTM().GetInverted().TransformPoint(worldRefPoint);
			}
		}
		else
		{
			SDebugInfo newTarget;
			newTarget.m_targetId = entityId;
			newTarget.m_visible = visible;
			newTarget.m_lastUpdatedTime = gEnv->pTimer->GetCurrTime();
			if (updateRefPoint)
			{
				newTarget.m_localTargetPos = pEntity->GetWorldTM().GetInverted().TransformPoint(worldRefPoint);
			}

			m_debugTargets.push_back(newTarget);
		}
	}
}

CPlayerVisTableDebugDraw::SDebugInfo* CPlayerVisTableDebugDraw::GetDebugInfoForEntity(const EntityId targetId)
{
	TDebugTargets::iterator targetIt = m_debugTargets.begin();
	TDebugTargets::iterator targetsEnd = m_debugTargets.end();

	while((targetIt != targetsEnd) && (targetIt->m_targetId != targetId))
	{
		++targetIt;
	}

	return (targetIt != targetsEnd) ? &(*targetIt) : NULL;

}

void CPlayerVisTableDebugDraw::Update()
{
	const float currentTime = gEnv->pTimer->GetCurrTime();
	const float maxDebugLifeTime = 1.0f;

	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();
	const ColorB visibleColor(0, 255, 0, 128);
	const ColorB hiddenColor(255, 0, 0, 128);
	const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	SAuxGeomRenderFlags oldRenderFlags = pRenderAux->GetRenderFlags();
	SAuxGeomRenderFlags newRenderFlags = e_Def3DPublicRenderflags;
	newRenderFlags.SetAlphaBlendMode(e_AlphaBlended);
	newRenderFlags.SetDepthTestFlag(e_DepthTestOff);
	newRenderFlags.SetCullMode(e_CullModeNone); 
	pRenderAux->SetRenderFlags(newRenderFlags);

	TDebugTargets::iterator targetIt = m_debugTargets.begin();
	while (targetIt != m_debugTargets.end())
	{
		SDebugInfo& targetInfo = *targetIt;

		const float lastUpdateAge = (currentTime - targetInfo.m_lastUpdatedTime);
		const bool remove = (lastUpdateAge > maxDebugLifeTime);

		if (!remove)
		{
			IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(targetInfo.m_targetId);
			if (pTargetEntity)
			{
				const ColorB& color = targetInfo.m_visible ? visibleColor : hiddenColor;
				const Vec3 worldRefPosition = pTargetEntity->GetWorldTM().TransformPoint(targetInfo.m_localTargetPos);

				const Vec3 offset(0.0f, 0.0f, 0.4f);
				pRenderAux->DrawCone(worldRefPosition + offset, -Vec3Constants<float>::fVec3_OneZ, 0.125f, offset.z, color);
				IRenderAuxText::DrawLabelExF(worldRefPosition, 1.5f, white, true, false, "%.2f", lastUpdateAge);
			}
			++targetIt;
		}
		else
		{
			TDebugTargets::iterator nextElement = m_debugTargets.erase(targetIt);
			targetIt = nextElement;
		}
	}

	pRenderAux->SetRenderFlags(oldRenderFlags);
}
#endif