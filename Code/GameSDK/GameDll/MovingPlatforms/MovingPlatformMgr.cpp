// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovingPlatforms/MovingPlatformMgr.h"
#include "CorpseManager.h"
#include "Utility/CryWatch.h"
#include "EnvironmentalWeapon.h"
#include "GameRulesModules/IGameRulesObjectivesModule.h"
#include "MultiplayerEntities/CarryEntity.h"

//------------------------------------------------------------------------------------------
CMovingPlatformMgr::CMovingPlatformMgr()
{
	const int kLogged = 1;
	gEnv->pPhysicalWorld->AddEventClient(EventPhysCollision::id, StaticOnCollision, kLogged);
	gEnv->pPhysicalWorld->AddEventClient(EventPhysEntityDeleted::id, StaticOnDeleted, kLogged);
}

//------------------------------------------------------------------------------------------
CMovingPlatformMgr::~CMovingPlatformMgr()
{
	const int kLogged = 1;
	gEnv->pPhysicalWorld->RemoveEventClient(EventPhysCollision::id, StaticOnCollision, kLogged);
	gEnv->pPhysicalWorld->RemoveEventClient(EventPhysEntityDeleted::id, StaticOnDeleted, kLogged);
}

//------------------------------------------------------------------------------------------
int CMovingPlatformMgr::OnCollisionLogged( const EventPhys* pEvent )
{
	static const IEntityClass* pCorpseClass =  gEnv->pEntitySystem->GetClassRegistry()->FindClass("Corpse");

	const EventPhysCollision* pCollision = static_cast<const EventPhysCollision*>(pEvent);

	// Get Foreign Data.
	pe_params_foreign_data ppf[2];
	if(!pCollision->pEntity[0]->GetParams(&ppf[0]))
		return 1;
	if(!pCollision->pEntity[1]->GetParams(&ppf[1]))
		return 1;

	// Only want one Platform, and both should be Entity type.
	if(	!((ppf[0].iForeignFlags ^ ppf[1].iForeignFlags)&PFF_MOVING_PLATFORM) ||
			ppf[0].iForeignData!=PHYS_FOREIGN_ID_ENTITY ||
			ppf[1].iForeignData!=PHYS_FOREIGN_ID_ENTITY )
		return 1;

	// Get Platform and Passenger indices.
	const int PLAT = iszero(ppf[0].iForeignFlags&PFF_MOVING_PLATFORM);
	const int PASS = 1-PLAT;

	// Platform must be Rigid.
	IPhysicalEntity* pPlatPhys = pCollision->pEntity[PLAT];
	if (pPlatPhys->GetType() != PE_RIGID)
		return 1;

	IEntity* pPlatEntity = (IEntity*)ppf[PLAT].pForeignData;
	IEntity* pPassEntity = (IEntity*)ppf[PASS].pForeignData;
	if(!pPlatEntity || !pPassEntity)
	{
		return 1; 
	}

	// Passenger must be Rigid (objects) or Articulated Corpse.
	IPhysicalEntity* pPassPhys = pCollision->pEntity[PASS];
	const pe_type passengerType = pPassPhys->GetType();
	const EntityId passengerEntityId = pPassEntity->GetId();

	if( passengerType==PE_ARTICULATED || pPassEntity->GetClass()==pCorpseClass )
	{
		CCorpseManager* pCorpseManager = g_pGame->GetGameRules()->GetCorpseManager();
		pCorpseManager->KeepAwake(passengerEntityId, pPassPhys);
		return 1;
	}

	const bool bLocalPassenger = (pPassEntity->GetFlags()&ENTITY_FLAG_CLIENT_ONLY)!=0;

	if (!gEnv->bServer || bLocalPassenger)
		return 1;
	
	if ( passengerType!=PE_RIGID )
		return 1;

	// Do we have an upward collision normal?
	static float scale[2] = {-1.f, +1.f};
	if ((pCollision->n.z * scale[PLAT]) < 0.3f)
		return 1;
	
	TContactList::iterator it=m_contacts.find(passengerEntityId);
	if(it!=m_contacts.end())
	{
		// Existing Contact.
		it->second.Refresh();
		return 1;
	}

	IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
	if(IItem* pItem = pGameFramework->GetIItemSystem()->GetItem(passengerEntityId))
	{
		if(pItem->GetOwnerId())
		{
			return 1;
		}
	}
	else if(CEnvironmentalWeapon* pEnvWeap = static_cast<CEnvironmentalWeapon*>(pGameFramework->QueryGameObjectExtension(passengerEntityId, "EnvironmentalWeapon")))
	{
		if(pEnvWeap->GetOwner())
		{
			return 1;
		}
	}
	else
	{
		return 1;
	}

	// New contact
	SContact& contact = m_contacts[passengerEntityId];
	contact.fTimeSinceFirstContact = 0.f;
	contact.fTimeSinceLastContact = 0.f;

	return 1;
}
	
//------------------------------------------------------------------------------------------
int CMovingPlatformMgr::OnDeletedLogged( const EventPhys* pEvent )
{
	const EventPhysEntityDeleted* pEventDeleted = static_cast<const EventPhysEntityDeleted*>(pEvent);
	if(pEventDeleted->iForeignData==PHYS_FOREIGN_ID_ENTITY)
	{
		if(IEntity* pEntity = (IEntity*)pEventDeleted->pForeignData)
		{
			TContactList::iterator it=m_contacts.find(pEntity->GetId());
			if(it!=m_contacts.end())
			{
				m_contacts.erase(it);
			}
		}
	}
	return 1;
}

//------------------------------------------------------------------------------------------
void CMovingPlatformMgr::Update( const float dt )
{
	if( !gEnv->bServer || m_contacts.empty() )
		return;

	for(TContactList::iterator it=m_contacts.begin(); it!=m_contacts.end(); )
	{
		const EntityId entityId = it->first;
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
		if(!pEntity)
		{
			// Invalid entity
			m_contacts.erase(it++);
		}
		else if(!pEntity->GetPhysics())
		{
			// Invalid Physics.
			m_contacts.erase(it++);
		}
		else
		{
			SContact& contact = it->second;
			contact.Update(dt);

			if( contact.fTimeSinceFirstContact > 4.f )
			{
				// Been on here too long!
				gEnv->pEntitySystem->RemoveEntity(pEntity->GetId());
				m_contacts.erase(it++);
			}
			else if( contact.fTimeSinceLastContact > 0.5f )
			{
				// Probably fell off or went to sleep.
				m_contacts.erase(it++);
			}
			else
			{
				// Valid.
				//CryWatch("%s: [%.2f / %.2f]", pEntity->GetName(), contact.fTimeSinceFirstContact, contact.fTimeSinceLastContact);
				++it;
			}
		}
	}
}
	
//------------------------------------------------------------------------------------------
void CMovingPlatformMgr::Reset()
{
	// Delete all the contacts
	m_contacts.clear();
}
	
//------------------------------------------------------------------------------------------
// Static Physics Event Receivers
//------------------------------------------------------------------------------------------

int CMovingPlatformMgr::StaticOnCollision( const EventPhys * pEvent )
{
	return g_pGame->GetMovingPlatformManager()->OnCollisionLogged(pEvent);
}

int CMovingPlatformMgr::StaticOnDeleted( const EventPhys * pEvent )
{
	return g_pGame->GetMovingPlatformManager()->OnDeletedLogged(pEvent);
}

