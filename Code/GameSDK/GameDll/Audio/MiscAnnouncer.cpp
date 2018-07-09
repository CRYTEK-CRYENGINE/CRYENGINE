// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

	Plays Announcements based upon general game events

History:
- 17:11:2012		Created by Jim Bamford

*************************************************************************/

#include "StdAfx.h"
#include "Audio/MiscAnnouncer.h"
#include "GameRules.h"
#include "IActorSystem.h"
#include "Player.h"
#include "Announcer.h"
#include "Weapon.h"
#include "Utility/CryWatch.h"
#include "WeaponSystem.h"

int static ma_debug = 0;

int static ma_enabled = 1;

#if !defined(_RELEASE) && !CRY_PLATFORM_ORBIS
	#define DbgLog(...) \
		if(ma_debug >= 1) \
		{ \
            CryLogAlways((string().Format("<MA> " __VA_ARGS__)).c_str()); \
		}
#else
	#define DbgLog(...) (void)0
#endif


//---------------------------------------
CMiscAnnouncer::CMiscAnnouncer() 
{
	CryLog("CMiscAnnouncer::CMiscAnnouncer()");

	REGISTER_CVAR(ma_enabled, ma_enabled, VF_NULL, "Stops misc announcements being played or updated");
	REGISTER_CVAR_DEV_ONLY(ma_debug, ma_debug, VF_NULL, "Enable/Disables Misc announcer debug messages: >= 1 - verbose logging; 2 - debug onWeaponFired map; 3 - debug actor listener map");
}

//---------------------------------------
CMiscAnnouncer::~CMiscAnnouncer()
{
	CryLog("CMiscAnnouncer::~CMiscAnnouncer()");

	if(gEnv->pConsole)
	{
		gEnv->pConsole->UnregisterVariable("ma_enabled");
#if !defined(_RELEASE)
		gEnv->pConsole->UnregisterVariable("ma_debug");
#endif
	}

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(pGameRules)
	{
		pGameRules->UnRegisterRoundsListener(this);
		pGameRules->RemoveGameRulesListener(this);
	}

	RemoveAllWeaponListeners();	
	g_pGame->GetIGameFramework()->GetIItemSystem()->UnregisterListener(this);
}

//---------------------------------------
void CMiscAnnouncer::Init()
{
	CryLog("CMiscAnnouncer::Init()");

	IEntityClassRegistry *pEntityClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	
	XmlNodeRef xmlData = GetISystem()->LoadXmlFromFile("Scripts/Sounds/MiscAnnouncements.xml");
	InitXML(xmlData);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	pGameRules->RegisterRoundsListener(this);
	pGameRules->AddGameRulesListener(this);

	g_pGame->GetIGameFramework()->GetIItemSystem()->RegisterListener(this);
}

//---------------------------------------
void CMiscAnnouncer::Reset()
{
	CryLog("CMiscAnnouncer::Reset()");

	for (TWeaponFiredMap::iterator it=m_weaponFiredMap.begin(); it != m_weaponFiredMap.end(); ++it)
	{
		SOnWeaponFired &onWeaponFired = it->second;
		onWeaponFired.Reset();
	}
}

//---------------------------------------
void CMiscAnnouncer::Update(const float dt)
{
	if(!ma_enabled || !AnnouncerRequired())
		return;

#if !defined(_RELEASE)
	switch(ma_debug)
	{
		case 2:    // debug OnWeaponFiredMap
			for (TWeaponFiredMap::iterator it=m_weaponFiredMap.begin(); it != m_weaponFiredMap.end(); ++it)
			{
				SOnWeaponFired &onWeaponFired = it->second;
				CryWatch("weaponFired: weapon=%s; announce=%s; played: friend=%s; enemy=%s", onWeaponFired.m_weaponEntityClass->GetName(), onWeaponFired.m_announcementName.c_str(), onWeaponFired.m_havePlayedFriendly ? "true" : "false", onWeaponFired.m_havePlayedEnemy ? "true" : "false");
			}
			break;
		case 3:			// debug listeners
		{
			CGameRules *pGameRules = g_pGame->GetGameRules();
			for (ActorWeaponListenerMap::iterator it=m_actorWeaponListener.begin(); it != m_actorWeaponListener.end(); ++it)
			{
				CryWatch("ActorWeaponListener: Actor=%s; Weapon=%s", pGameRules->GetEntityName(it->first), pGameRules->GetEntityName(it->second));
			}
		}
	}
#endif
}

// SGameRulesListener
//---------------------------------------
void CMiscAnnouncer::ClientDisconnect( EntityId clientId )
{
	DbgLog("CMiscAnnouncer::ClientDisconnect() clientId=%s", g_pGame->GetGameRules()->GetEntityName(clientId));
	RemoveWeaponListenerForActor(clientId);
}

// IGameRulesRoundsListener
//---------------------------------------
void CMiscAnnouncer::OnRoundStart()
{
	CryLog("CMiscAnnouncer::OnRoundStart() resetting");
	Reset();
}

//IItemSystemListener
//---------------------------------------
void CMiscAnnouncer::OnSetActorItem(IActor *pActor, IItem *pItem )
{
	if( pItem && pActor && pActor->IsPlayer() )
	{
		IWeapon* pWeapon=pItem->GetIWeapon();
		DbgLog("CMiscAnnouncer::OnSetActorItem() pActor=%s; pItem=%s; pWeapon=%p", pActor->GetEntity()->GetName(), pItem->GetDisplayName(), pWeapon);

		if(pWeapon)
		{
			EntityId actorId = pActor->GetEntityId();
			EntityId weaponId = pItem->GetEntityId();
			SetNewWeaponListener(pWeapon, weaponId, actorId);
		}
	}
}

// IWeaponEventListener
//---------------------------------------
void CMiscAnnouncer::OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel)
{
	if(!ma_enabled || !AnnouncerRequired())
		return;

	CWeapon *pWeaponImpl = static_cast<CWeapon*>(pWeapon);
	IEntityClass *pWeaponEntityClass = pWeaponImpl->GetEntity()->GetClass();
	g_pGame->GetWeaponSystem()->GetWeaponAlias().UpdateClass(&pWeaponEntityClass);

	DbgLog("CMiscAnnouncer::OnShoot() pWeaponImpl=%s; shooter=%d; pAmmoType=%s", pWeaponImpl->GetEntity()->GetName(), g_pGame->GetGameRules()->GetEntityName(shooterId), pAmmoType->GetName());
	
	TWeaponFiredMap::iterator it = m_weaponFiredMap.find(pWeaponEntityClass);
	if(it != m_weaponFiredMap.end())
	{
		SOnWeaponFired &onWeaponFired = it->second;

		DbgLog("CMiscAnnouncer::OnShoot() has found the firing weaponClass in our weaponFiredMap. With announcement=%s", onWeaponFired.m_announcementName.c_str());

		// we only want to play the announcement once each game/round
		IActor *pClientActor = gEnv->pGameFramework->GetClientActor();
		CGameRules *pGameRules = g_pGame->GetGameRules();
		int clientTeam = pGameRules->GetTeam(pClientActor->GetEntityId());
		int shooterTeam = pGameRules->GetTeam(shooterId);
		
		if (clientTeam == shooterTeam)
		{
			if (!onWeaponFired.m_havePlayedFriendly)
			{
				DbgLog("CMiscAnnouncer::OnShoot() we've not played this friendly annoucement already. Let's do it");
				CAnnouncer::GetInstance()->Announce(shooterId, onWeaponFired.m_announcementID, CAnnouncer::eAC_inGame);
			}
			else
			{
				DbgLog("CMiscAnnouncer::OnShoot() we've already played this friendly announcement. Not playing again");
			}
			onWeaponFired.m_havePlayedFriendly = true;
		}
		else
		{
			if (!onWeaponFired.m_havePlayedEnemy)
			{
				DbgLog("CMiscAnnouncer::OnShoot() we've not played this enemy announcement already. Let's do it");
				CAnnouncer::GetInstance()->Announce(shooterId, onWeaponFired.m_announcementID, CAnnouncer::eAC_inGame);
			}
			else
			{
				DbgLog("CMiscAnnouncer::OnShoot() we've already played this enemy announcement. Not playing again.");
			}
			onWeaponFired.m_havePlayedEnemy = true;
		}
	}
}

//---------------------------------------
void CMiscAnnouncer::InitXML(XmlNodeRef root)
{
	IEntityClassRegistry *pEntityClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	XmlNodeRef onWeaponFiringRoot = root->findChild("OnWeaponFiring");

	CryLog("CMiscAnnouncer::InitXML()");

	if(onWeaponFiringRoot)
	{
		const int numChildren = onWeaponFiringRoot->getChildCount();

		CryLog("CMiscAnnouncer::InitXML() found OnWeaponFiringRoot with %d children", numChildren);
	
		for(int i=0; i<numChildren; ++i)
		{
			XmlNodeRef child = onWeaponFiringRoot->getChild(i);
			if(child->isTag("OnWeaponFired"))
			{
				const char *pWeaponClassName = child->getAttr("weaponClass");
				CRY_ASSERT(pWeaponClassName && pWeaponClassName[0] != 0);

				CryLog("CMiscAnnouncer::InitXML() OnWeaponFired tag - pWeaponClassName=%s", pWeaponClassName ? pWeaponClassName : "NULL");

				if(IEntityClass* pWeaponEntityClass = pEntityClassRegistry->FindClass(pWeaponClassName))
				{
					const char *pAnnouncement = child->getAttr("announcement");
					CRY_ASSERT(pAnnouncement && pAnnouncement[0] != 0);

					EAnnouncementID announcementID = CAnnouncer::NameToID(pAnnouncement);
					
					CryLog("CMiscAnnouncer::InitXML() found weapon entity class for pWeaponClassName=%s; found pAnnouncement=%s announcementID=%x", pWeaponClassName, pAnnouncement ? pAnnouncement : "NULL", announcementID);

					SOnWeaponFired newWeaponFired(pWeaponEntityClass, pAnnouncement, announcementID);
					m_weaponFiredMap.insert(TWeaponFiredMap::value_type(pWeaponEntityClass, newWeaponFired));
				}
				else
				{
					CryLog("CMiscAnnouncer::InitXML() failed to find entityClass for pWeaponClassName=%s", pWeaponClassName);
					CRY_ASSERT_MESSAGE(0, string().Format("CMiscAnnouncer::InitXML() failed to find entityClass for pWeaponClassName=%s", pWeaponClassName));
				}
			}
			else
			{
				CryLog("CMiscAnnouncer::InitXML() unhandled childtag of %s found", child->getTag());
			}
		}
	}
}

//---------------------------------------
void CMiscAnnouncer::SetNewWeaponListener(IWeapon* pWeapon, EntityId weaponId, EntityId actorId)
{
	DbgLog("CMiscAnnouncer::SetNewWeaponListener() pWeapon=%p; actor=%s", pWeapon, g_pGame->GetGameRules()->GetEntityName(actorId));

	ActorWeaponListenerMap::iterator it = m_actorWeaponListener.find(actorId);
	if(it != m_actorWeaponListener.end())
	{
		//remove previous weapon listener for actor
		RemoveWeaponListener(it->second);
		//update with new weapon
		it->second = weaponId;
	}
	else
	{
		//aren't listener so add actor and weapon
		m_actorWeaponListener.insert(ActorWeaponListenerMap::value_type(actorId, weaponId));
	}
	
	pWeapon->AddEventListener(this, "CMiscAnnouncer");
}

//---------------------------------------
void CMiscAnnouncer::RemoveWeaponListener(EntityId weaponId)
{
	DbgLog("CMiscAnnouncer::RemoveWeaponListener() weapon=%s", g_pGame->GetGameRules()->GetEntityName(weaponId));

	IItem* pItem = gEnv->pGameFramework->GetIItemSystem()->GetItem(weaponId);
	if(pItem)
	{
		IWeapon *pWeapon = pItem->GetIWeapon();
		if(pWeapon)
		{
			pWeapon->RemoveEventListener(this);
		}
	}
}

//---------------------------------------
void CMiscAnnouncer::RemoveAllWeaponListeners()
{
	CryLog("CMiscAnnouncer::RemoveAllWeaponListeners()");

	ActorWeaponListenerMap::const_iterator it = m_actorWeaponListener.begin();
	ActorWeaponListenerMap::const_iterator end = m_actorWeaponListener.end();
	for ( ; it!=end; ++it)
	{
		RemoveWeaponListener(it->second);
	}

	m_actorWeaponListener.clear();
}

//---------------------------------------
void CMiscAnnouncer::RemoveWeaponListenerForActor(EntityId actorId)
{
	DbgLog("CMiscAnnouncer::RemoveWeaponListenerForActor() actor=%s", g_pGame->GetGameRules()->GetEntityName(actorId));

	ActorWeaponListenerMap::iterator it = m_actorWeaponListener.find(actorId);
	if(it != m_actorWeaponListener.end())
	{
		//remove previous weapon listener for actor
		RemoveWeaponListener(it->second);
	}
}


//---------------------------------------
bool CMiscAnnouncer::AnnouncerRequired()
{
	bool required=true;

	if (gEnv->IsDedicated())
	{
		required=false;
	}

#if 0
	EGameMode gameMode = g_pGame->GetGameRules()->GetGameMode();

	switch(gameMode)
	{
		case eGM_Gladiator:
		case eGM_Assault:
			required=false;
			break;
	}
#endif
	return required;
}

#undef DbgLog
