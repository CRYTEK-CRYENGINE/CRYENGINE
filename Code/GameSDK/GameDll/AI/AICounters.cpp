// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
 Helper to calculate and centralize global game side values related to AI actors
*************************************************************************/

#include "StdAfx.h"
#include "AICounters.h"
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAIObjectManager.h>
#include <CryAISystem/IAIActorProxy.h>
#include <CryAISystem/IFactionMap.h>

//////////////////////////////////////////////////////////////////////////
void CAICounters::Reset( bool bUnload )
{
	m_alertnessCounter.Reset( bUnload );
}


//////////////////////////////////////////////////////////////////////////
void CAICounters::Serialize( TSerialize ser )
{
	m_alertnessCounter.Serialize( ser );
}


//////////////////////////////////////////////////////////////////////////
void CAICounters::Update( float frameTime )
{
	m_alertnessCounter.Update( frameTime );
}



// ##########################################################################################

const float CAICounter_Alertness::UPDATE_INTERVAL = 0.8f;    // seconds

//////////////////////////////////////////////////////////////////////////
CAICounter_Alertness::CAICounter_Alertness() 
	: m_bFactionVectorsAreValid(false)
	, m_bNewListeners(false)
	, m_bJustUpdated(false)
	, m_timeNextUpdate(0.0f)
	, m_alertnessGlobal(-1)
	, m_alertnessFriends(-1)
	, m_alertnessEnemies(-1)
{
	m_listeners.reserve(16);
}

//////////////////////////////////////////////////////////////////////////
void CAICounter_Alertness::Reset( bool bUnload )
{
	m_timeNextUpdate = 0.0f;
	m_alertnessGlobal = -1;  // to force an update at least once, even if the alertness is 0
	m_alertnessFriends = -1;
	m_alertnessEnemies = -1;
	m_bFactionVectorsAreValid = false;
	m_bNewListeners = false;
	m_bJustUpdated = false;

	stl::free_container(m_alertnessFaction);
	stl::free_container(m_tempAlertnessFaction);
}


//////////////////////////////////////////////////////////////////////////
void CAICounter_Alertness::Update( float frameTime )
{
	m_bJustUpdated = false;	
	if (m_listeners.empty())
		return;

	float currTime = gEnv->pTimer->GetCurrTime();
	if (currTime<m_timeNextUpdate)
		return;

	m_timeNextUpdate = currTime + UPDATE_INTERVAL;

	UpdateCounters();
}


//////////////////////////////////////////////////////////////////////////
void CAICounter_Alertness::UpdateCounters()
{
	const IActor*     pLocalPlayer    = gEnv->pGameFramework->GetClientActor();
	const IAIObject*  pAILocalPlayer  = pLocalPlayer ? pLocalPlayer->GetEntity()->GetAI() : NULL;
	if (!pAILocalPlayer)
		return;

	m_bJustUpdated = true;

	if (!m_bFactionVectorsAreValid)
	{
		m_alertnessFaction.clear();
		m_alertnessFaction.resize( gEnv->pAISystem->GetFactionMap().GetFactionCount(), 0 );
		m_tempAlertnessFaction.resize( gEnv->pAISystem->GetFactionMap().GetFactionCount(), 0 );
		m_bFactionVectorsAreValid = true;
	}

	for (std::vector<int>::iterator iter = m_tempAlertnessFaction.begin(); iter!=m_tempAlertnessFaction.end(); ++iter)
		(*iter)=0;
	int alertnessGlobal = 0;
	int alertnessFriends = 0;
	int alertnessEnemies = 0;


	IActorIteratorPtr actorIt = gEnv->pGameFramework->GetIActorSystem()->CreateActorIterator();
	while (IActor *pActor=actorIt->Next())
	{
		if (pActor && pActor!=pLocalPlayer)
		{
			const IAIObject* pAIObject = pActor->GetEntity()->GetAI();
			if (pAIObject)
			{
				const IAIActor* pAIActor = pAIObject->CastToIAIActor();
				CRY_ASSERT(pAIActor);
				if (pAIActor && pAIActor->IsActive())
				{
					int alertness = pAIActor->GetProxy()->GetAlertnessState();
					alertnessGlobal = max( alertnessGlobal, alertness );

					uint32 faction = pAIObject->GetFactionID();
					m_tempAlertnessFaction[ faction ] = max( m_tempAlertnessFaction[ faction ], alertness );
					bool enemy = pAIObject->IsHostile( pAILocalPlayer );
					if (enemy)
						alertnessEnemies = max( alertnessEnemies, alertness );
					else
						alertnessFriends = max( alertnessFriends, alertness );
				}
			}
		}
	}

	bool hasChanged = false;
	if (m_alertnessGlobal!=alertnessGlobal || m_alertnessEnemies!=alertnessEnemies || m_alertnessFriends!=alertnessFriends)
	{
		hasChanged = true;
	}
	else
	{
		std::vector<int>::iterator iterTemp = m_tempAlertnessFaction.begin();
		std::vector<int>::iterator iter = m_alertnessFaction.begin();
		for (; iterTemp!=m_tempAlertnessFaction.end(); ++iterTemp, ++iter)
		{
			int valTemp = *iterTemp;
			int val = *iter;
			if (valTemp!=val)
			{
				hasChanged = true;
				break;
			}
		}
	}

	if (hasChanged || m_bNewListeners)
	{
		m_alertnessGlobal = alertnessGlobal;
		m_alertnessEnemies = alertnessEnemies;
		m_alertnessFriends = alertnessFriends;
		m_alertnessFaction.swap( m_tempAlertnessFaction );
		NotifyListeners();
		m_bNewListeners = false;
	}
}


//////////////////////////////////////////////////////////////////////////
int CAICounter_Alertness::GetAlertnessGlobal() 
{
	InstantUpdateIfNeed();
	return m_alertnessGlobal; 
}

//////////////////////////////////////////////////////////////////////////
int CAICounter_Alertness::GetAlertnessEnemies() 
{ 
	InstantUpdateIfNeed();
	return m_alertnessEnemies; 
}

//////////////////////////////////////////////////////////////////////////
int CAICounter_Alertness::GetAlertnessFriends() 
{ 
	InstantUpdateIfNeed();
	return m_alertnessFriends; 
}


//////////////////////////////////////////////////////////////////////////
int CAICounter_Alertness::GetAlertnessFaction( uint32 factionID ) 
{ 
	InstantUpdateIfNeed();
	CRY_ASSERT(factionID<m_alertnessFaction.size());
	if (factionID<m_alertnessFaction.size())
		return m_alertnessFaction[factionID]; 
	else
		return 0;
}


//////////////////////////////////////////////////////////////////////////
void CAICounter_Alertness::InstantUpdateIfNeed() 
{ 
	if (!m_bJustUpdated)
		UpdateCounters();
}

//////////////////////////////////////////////////////////////////////////
void CAICounter_Alertness::AddListener(CAICounter_Alertness::IListener *pListener)
{
	stl::push_back_unique( m_listeners, pListener );
	m_bNewListeners = true;
}

//////////////////////////////////////////////////////////////////////////
void CAICounter_Alertness::RemoveListener(CAICounter_Alertness::IListener *pListener)
{
	stl::find_and_erase( m_listeners, pListener );
}

//////////////////////////////////////////////////////////////////////////
void CAICounter_Alertness::NotifyListeners()
{
	if (!m_listeners.empty())
	{
		TListenersVector::iterator iter = m_listeners.begin();
		while (iter != m_listeners.end())
		{
			IListener* pListener = *iter;
			pListener->AlertnessChanged( m_alertnessGlobal );
			++iter;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CAICounter_Alertness::Serialize( TSerialize ser )
{
	ser.BeginGroup("AICounter_Alertness");

	ser.Value( "m_timeNextUpdate", m_timeNextUpdate );
	ser.Value( "m_alertnessGlobal", m_alertnessGlobal );
	ser.Value( "m_alertnessEnemies", m_alertnessEnemies );
	ser.Value( "m_alertnessFriends", m_alertnessFriends );
	ser.Value( "m_alertnessFaction", m_alertnessFaction );
	ser.Value( "m_bNewListeners", m_bNewListeners );
	if (ser.IsReading())
	{
		m_bJustUpdated = false;
		m_bFactionVectorsAreValid = false;
	}

	ser.EndGroup();
}

