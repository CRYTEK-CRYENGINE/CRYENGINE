// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MPPathFollowingManager.h"
#include "GameRules.h"

CMPPathFollowingManager::CMPPathFollowingManager()
{
	m_Paths.reserve(4);
}

CMPPathFollowingManager::~CMPPathFollowingManager()
{

}

void CMPPathFollowingManager::RegisterClassFollower(uint16 classId, IMPPathFollower* pFollower)
{
#ifndef _RELEASE
	PathFollowers::iterator iter = m_PathFollowers.find(classId);
	CRY_ASSERT_MESSAGE(iter == m_PathFollowers.end(), "CMPPathFollowingManager::RegisterClassFollower - this class has already been registered!");
#endif
	m_PathFollowers[classId] = pFollower;
}

void CMPPathFollowingManager::UnregisterClassFollower(uint16 classId)
{
	PathFollowers::iterator iter = m_PathFollowers.find(classId);
	if(iter != m_PathFollowers.end())
	{
		m_PathFollowers.erase(iter);
		return;
	}

	CRY_ASSERT_MESSAGE(false, "CMPPathFollowingManager::UnregisterClassFollower - tried to unregister class but class not found");
}

void CMPPathFollowingManager::RequestAttachEntityToPath( const SPathFollowingAttachToPathParameters& params )
{
	PathFollowers::const_iterator iter = m_PathFollowers.find(params.classId);
	if(iter != m_PathFollowers.end())
	{
		CRY_ASSERT_MESSAGE(params.pathIndex < m_Paths.size(), "CMPPathFollowingManager::RequestAttachEntityToPath - path index out of range");
		iter->second->OnAttachRequest(params, &m_Paths[params.pathIndex].path);
		if(gEnv->bServer)
		{
			SPathFollowingAttachToPathParameters sendParams(params);
			sendParams.forceSnap = true;
			g_pGame->GetGameRules()->GetGameObject()->InvokeRMI(CGameRules::ClPathFollowingAttachToPath(), params, eRMI_ToRemoteClients);
		}
	}
}

void CMPPathFollowingManager::RequestUpdateSpeed(uint16 classId, EntityId attachEntityId, float newSpeed)
{
	PathFollowers::const_iterator iter = m_PathFollowers.find(classId);
	if(iter != m_PathFollowers.end())
	{
		iter->second->OnUpdateSpeedRequest(attachEntityId, newSpeed);
	}
}

bool CMPPathFollowingManager::RegisterPath(EntityId pathEntityId)
{
	IEntity* pPathEntity = gEnv->pEntitySystem->GetEntity(pathEntityId);
	if(pPathEntity)
	{
#ifndef _RELEASE
		Paths::const_iterator iter = m_Paths.begin();
		Paths::const_iterator end = m_Paths.end();
		while(iter != end)
		{
			if(iter->pathId == pathEntityId)
			{
				CRY_ASSERT_MESSAGE(iter == m_Paths.end(), "CMPPathFollowingManager::RegisterPath - this path has already been registered!");
				break;
			}
			++iter;
		}
#endif

		m_Paths.push_back( SPathEntry(pathEntityId) );
		bool success = m_Paths.back().path.CreatePath(pPathEntity);
		if(success)
		{
			//Editor support for changing path nodes
			if(gEnv->IsEditor())
			{
				gEnv->pEntitySystem->AddEntityEventListener(pathEntityId, ENTITY_EVENT_RESET, this);
			}

			return true;
		}
		else
		{
			m_Paths.pop_back();
		}
	}

	return false;
}

void CMPPathFollowingManager::UnregisterPath(EntityId pathEntityId)
{
	Paths::iterator iter = m_Paths.begin();
	Paths::iterator end = m_Paths.end();
	while(iter != end)
	{
		if(iter->pathId == pathEntityId)
		{
			if(gEnv->IsEditor())
			{
				gEnv->pEntitySystem->RemoveEntityEventListener(pathEntityId, ENTITY_EVENT_RESET, this);
			}
			m_Paths.erase(iter);
			return;
		}
		++iter;
	}
}

void CMPPathFollowingManager::RegisterListener(EntityId listenToEntityId, IMPPathFollowingListener* pListener)
{
#ifndef _RELEASE
	PathListeners::iterator iter = m_PathListeners.find(listenToEntityId);
	CRY_ASSERT_MESSAGE(iter == m_PathListeners.end(), "CMPPathFollowingManager::RegisterListener - this listener has already been registered!");
#endif

	m_PathListeners[listenToEntityId] = pListener;
}

void CMPPathFollowingManager::UnregisterListener(EntityId listenToEntityId)
{
	PathListeners::iterator iter = m_PathListeners.find(listenToEntityId);
	if(iter != m_PathListeners.end())
	{
		m_PathListeners.erase(iter);
		return;
	}

	CRY_ASSERT_MESSAGE(false, "CMPPathFollowingManager::UnregisterListener - tried to unregister listener but listener not found");
}

const CWaypointPath* CMPPathFollowingManager::GetPath(EntityId pathEntityId, IMPPathFollower::MPPathIndex& outIndex) const
{
	for(unsigned int i = 0; i < m_Paths.size(); ++i)
	{
		if(m_Paths[i].pathId == pathEntityId)
		{
			outIndex = i;
			return &m_Paths[i].path;
		}
	}

	return NULL;
}

void CMPPathFollowingManager::NotifyListenersOfPathCompletion(EntityId pathFollowingEntityId)
{
	PathListeners::iterator iter = m_PathListeners.find(pathFollowingEntityId);
	if(iter != m_PathListeners.end())
	{
		iter->second->OnPathCompleted(pathFollowingEntityId);
	}
}

 void CMPPathFollowingManager::OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
 {
		if(event.event == ENTITY_EVENT_RESET && event.nParam[0] == 0) //Only on leaving the editor gamemode
		{
			UnregisterPath(pEntity->GetId());
		}
 }

 #ifndef _RELEASE
 void CMPPathFollowingManager::Update()
 {
	 if(g_pGameCVars->g_mpPathFollowingRenderAllPaths)
	 {
		 Paths::const_iterator iter = m_Paths.begin();
		 Paths::const_iterator end = m_Paths.end();
		 while(iter != end)
		 {
			 iter->path.DebugDraw(true);
			 ++iter;
		 }
	 }
 }
#endif //_RELEASE
