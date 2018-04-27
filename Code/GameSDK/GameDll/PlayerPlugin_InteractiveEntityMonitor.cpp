// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PlayerPlugin_InteractiveEntityMonitor.h"

#include "Player.h"
#include "EnvironmentalWeapon.h"
#include "PlayerPluginEventDistributor.h"
#include "GameXmlParamReader.h"

#ifndef _RELEASE
#include "Utility/DesignerWarning.h"
#endif //_RELEASE

#define INTERACTIVE_ENTITY__MONITOR_DATA_FILE "Scripts/Entities/Multiplayer/InteractiveEntityMonitor.xml"

ColorF CPlayerPlugin_InteractiveEntityMonitor::m_silhouetteInteractColor = ColorF(0.f, 0.3f, 0.55f, 0.8f);
ColorF CPlayerPlugin_InteractiveEntityMonitor::m_silhouetteShootColor = ColorF(0.55f, 0.1f, 0.f, 0.8f);
 
CPlayerPlugin_InteractiveEntityMonitor::CPlayerPlugin_InteractiveEntityMonitor() : m_bEnabled(false), m_playerPrevPos(ZERO), m_timeUntilRefresh(0.f)
{
}

CPlayerPlugin_InteractiveEntityMonitor::~CPlayerPlugin_InteractiveEntityMonitor()
{
}

void CPlayerPlugin_InteractiveEntityMonitor::Enter( CPlayerPluginEventDistributor* pEventDist )
{
	CPlayerPlugin::Enter(pEventDist);

	pEventDist->RegisterEvents(this, EPE_Die, EPE_Spawn, EPE_InteractiveEntityRegister, EPE_InteractiveEntityRegisterShoot, EPE_InteractiveEntityUnregister, EPE_InteractiveEntitySetEnabled);
}

void CPlayerPlugin_InteractiveEntityMonitor::Update( const float dt )
{
	m_timeUntilRefresh -= dt;

#ifndef _RELEASE
	//Verify entity integrity
	InteractiveEntityDebugMap::iterator mapIter = m_debugMap.begin();
	InteractiveEntityDebugMap::iterator mapEnd = m_debugMap.end();
	while(mapIter != mapEnd)
	{
		if(!gEnv->pEntitySystem->GetEntity(mapIter->first))
		{
			CryLog("[ERROR] InteractiveEntityMonitor. About to crash. Registered entity no longer exists: '%s'. Tell Gary (Really this time).", mapIter->second.c_str());
			DesignerWarning(false, "[ERROR] InteractiveEntityMonitor. About to crash. Registered entity no longer exists: '%s'. Tell Gary (Really this time).", mapIter->second.c_str());
		}

		++mapIter;
	}
#endif //_RELEASE

	IEntitySystem* pEntitySys = gEnv->pEntitySystem;
	const Vec3& playerPos = GetOwnerPlayer()->GetEntity()->GetWorldTM().GetColumn3();
	if(m_bEnabled && (m_timeUntilRefresh < 0.f || playerPos.GetSquaredDistance2D(m_playerPrevPos) > g_pGameCVars->g_highlightingMovementDistanceToUpdateSquared))
	{
		for(InteractiveEntityList::iterator it = m_interactiveEntityList.begin(); it!=m_interactiveEntityList.end(); )
		{
			const EntityId entityId = it->first;
			IEntity* pEntity = pEntitySys->GetEntity(entityId);
			if(!pEntity)
			{
				it=m_interactiveEntityList.erase(it);
				continue;
			}

			if (IEntityRender* pIEntityRender = pEntity->GetRenderInterface())
			{
				const Vec3& entityPos = pEntity->GetWorldTM().GetColumn3();
				const float distSq = entityPos.GetSquaredDistance2D(playerPos);

				const bool withinHighlightDistance = distSq <= g_pGameCVars->g_highlightingMaxDistanceToHighlightSquared;
				if( withinHighlightDistance )
				{
					// Apply intensity fade over outer half distance
					float alpha = distSq * 2.0f - g_pGameCVars->g_highlightingMaxDistanceToHighlightSquared;
					alpha *= __fres(g_pGameCVars->g_highlightingMaxDistanceToHighlightSquared);
					alpha = 1.0f - clamp_tpl(alpha, 0.0f, 1.0f);

					if( (it->second & EIES_ShootToInteract) == 0 )
					{
						//pIEntityRender->SetHUDSilhouettesParams(m_silhouetteInteractColor.r*alpha, m_silhouetteInteractColor.g*alpha, m_silhouetteInteractColor.b*alpha, m_silhouetteInteractColor.a*alpha);
					}
					else
					{
						//pIEntityRender->SetHUDSilhouettesParams(m_silhouetteShootColor.r*alpha, m_silhouetteShootColor.g*alpha, m_silhouetteShootColor.b*alpha, m_silhouetteShootColor.a*alpha);
					}
					it->second |= EIES_Highlighted;
				}
				else if( it->second & EIES_Highlighted )
				{
					//pIEntityRender->SetHUDSilhouettesParams(0.f, 0.f, 0.f, 0.f);
					it->second &= ~EIES_Highlighted;
				}
			}

			++it;
		}

		m_playerPrevPos = playerPos;
		m_timeUntilRefresh = g_pGameCVars->g_highlightingTimeBetweenForcedRefresh;
	}
}

void CPlayerPlugin_InteractiveEntityMonitor::HandleEvent( EPlayerPlugInEvent theEvent, void * data )
{
	switch(theEvent)
	{
	case EPE_Die:
		{
			EnableHighlighting(false);
			break;
		}
	case EPE_Spawn:
		{
			EnableHighlighting(true);
			m_timeUntilRefresh = -1.f;
			break;
		}
	case EPE_InteractiveEntityRegister:
		{
			Register((IEntity*)data, 0);
			break;
		}
	case EPE_InteractiveEntityRegisterShoot:
		{
			Register((IEntity*)data, EIES_ShootToInteract);
			break;
		}
	case EPE_InteractiveEntityUnregister:
		{
			Unregister((IEntity*)data);
			break;
		}
	case EPE_InteractiveEntitySetEnabled:
		{
			bool enable = *(bool*)data;
			if(enable && !m_bEnabled)
			{
				EnableHighlighting(true);
				m_timeUntilRefresh = -1.f;
			}
			else if(!enable && m_bEnabled)
			{
				EnableHighlighting(false);
			}
			break;
		}
	}

	CPlayerPlugin::HandleEvent(theEvent, data);
}

void CPlayerPlugin_InteractiveEntityMonitor::Register( IEntity* pEntity, uint8 initialFlags )
{
	if(!pEntity)
		return;

	const EntityId entityId = pEntity->GetId();

	//Make sure we aren't registering twice
	InteractiveEntityList::iterator iter = m_interactiveEntityList.begin();
	InteractiveEntityList::iterator end = m_interactiveEntityList.end();
	while(iter != end)
	{
		if(iter->first == entityId)
		{
#ifndef _RELEASE
			CryLog("[ERROR] InteractiveEntityMonitor. Registering the same entity twice: '%s'. Tell Gary (Really this time).", pEntity->GetName());
			DesignerWarning(false, "InteractiveEntityMonitor. Registering the same entity twice: '%s'. Tell Gary (Really this time).", pEntity->GetName());
#endif //_RELEASE
			//Don't crash
			return;
		}
		++iter;
	}

#ifndef _RELEASE
	m_debugMap[pEntity->GetId()] = pEntity->GetName();
#endif //_RELEASE

	m_interactiveEntityList.push_back( InteractiveEntityStatus(entityId, initialFlags) );

	//Check if it should already be on
	if(m_bEnabled && GetOwnerPlayer()->GetEntity()->GetWorldPos().GetSquaredDistance2D(pEntity->GetWorldPos()) < g_pGameCVars->g_highlightingMovementDistanceToUpdateSquared)
	{
		if(IEntityRender* pIEntityRender = pEntity->GetRenderInterface())
		{
			if( (initialFlags & EIES_ShootToInteract) == 0 )
			{
				//pIEntityRender->SetHUDSilhouettesParams(m_silhouetteInteractColor.r, m_silhouetteInteractColor.g, m_silhouetteInteractColor.b, m_silhouetteInteractColor.a);
			}
			else
			{
				//pIEntityRender->SetHUDSilhouettesParams(m_silhouetteShootColor.r, m_silhouetteShootColor.g, m_silhouetteShootColor.b, m_silhouetteShootColor.a);
			}
		}
	}
}

void CPlayerPlugin_InteractiveEntityMonitor::Unregister( IEntity* pEntity )
{
	if(!pEntity)
		return;

	const EntityId entityId = pEntity->GetId();
	for(InteractiveEntityList::iterator it = m_interactiveEntityList.begin(); it!=m_interactiveEntityList.end(); )
	{
		if(it->first==entityId)
		{
			if(it->second)
			{
				if(IEntityRender* pIEntityRender = pEntity->GetRenderInterface())
				{
					//pIEntityRender->SetHUDSilhouettesParams(0.f, 0.f, 0.f, 0.0f);
				}
			}
#ifndef _RELEASE
			InteractiveEntityDebugMap::iterator result = m_debugMap.find(entityId);
			if(result != m_debugMap.end())
			{
				m_debugMap.erase(result);
			}
#endif //_RELEASE

			it = m_interactiveEntityList.erase(it);
			continue;
		}
		++it;
	}
}

void CPlayerPlugin_InteractiveEntityMonitor::EnableHighlighting(bool enable)
{
	m_bEnabled = enable;

	InteractiveEntityList::iterator iter = m_interactiveEntityList.begin();
	InteractiveEntityList::iterator end = m_interactiveEntityList.end();
	if(enable)
	{
		while(iter != end)
		{
			if( iter->second & EIES_Highlighted )
			{
				if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(iter->first))
				{
					if(IEntityRender* pIEntityRender = pEntity->GetRenderInterface())
					{
						if( (iter->second & EIES_ShootToInteract) == 0 )
						{
							//pIEntityRender->SetHUDSilhouettesParams(m_silhouetteInteractColor.r, m_silhouetteInteractColor.g, m_silhouetteInteractColor.b, m_silhouetteInteractColor.a);
						}
						else
						{
							//pIEntityRender->SetHUDSilhouettesParams(m_silhouetteShootColor.r, m_silhouetteShootColor.g, m_silhouetteShootColor.b, m_silhouetteShootColor.a);
						}
					}
				}
			}
			++iter;
		}
	}
	else
	{
		ColorF disableColor(0.f, 0.f, 0.f, 0.f);
		while(iter != end)
		{
			if( iter->second & EIES_Highlighted )
			{
				if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(iter->first))
				{
					if(IEntityRender* pIEntityRender = pEntity->GetRenderInterface())
					{
						//pIEntityRender->SetHUDSilhouettesParams(disableColor.r, disableColor.g, disableColor.b, disableColor.a);
					}
				}
			}
			++iter;
		}
	}
}

void CPlayerPlugin_InteractiveEntityMonitor::PrecacheLevel()
{
	XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromFile(INTERACTIVE_ENTITY__MONITOR_DATA_FILE);
	if(rootNode)
	{
		const char* pLevelName = gEnv->pGameFramework->GetLevelName();
		if(!pLevelName)
		{
			pLevelName = "default";
		}
		else if( const char * pTrim = strstr(pLevelName, "/") )
		{
			pLevelName = pTrim+1;
		}
		CGameXmlParamReader nodeDataReader(rootNode);
		XmlNodeRef levelNode = nodeDataReader.FindFilteredChild(pLevelName);
		if(!levelNode)
		{
			levelNode = nodeDataReader.FindFilteredChild("default");
		}
		if(levelNode)
		{
			ColorB color;
			if(levelNode->getAttr("color", color))
			{
				m_silhouetteInteractColor = ColorF(color.r, color.g, color.b, color.a) / 255.f;
			}
			if(levelNode->getAttr("shoot_color", color))
			{
				m_silhouetteShootColor = ColorF(color.r, color.g, color.b, color.a) / 255.f;
			}
		}
	}
}
