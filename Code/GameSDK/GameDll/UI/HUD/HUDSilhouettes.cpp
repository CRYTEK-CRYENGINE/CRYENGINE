// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 
	Silhouettes manager

-------------------------------------------------------------------------
History:
- 20:07:2007: Created by Julien Darre

*************************************************************************/

#include "StdAfx.h"
#include "IActorSystem.h"
#include <CryEntitySystem/IEntitySystem.h>
#include "IItemSystem.h"
#include "IVehicleSystem.h"
#include "HUDSilhouettes.h"
#include "Item.h"

#include "Actor.h"
#include <IUIDraw.h>

#include "GameCVars.h"
#include "Game.h" 
#include "Utility/CryWatch.h"
#include "HUDUtils.h"

bool CHUDSilhouettes::s_hasNotYetWarnedAboutVectorSize = true;

//-----------------------------------------------------------------------------------------------------

CHUDSilhouettes::CHUDSilhouettes() : m_heatVisionActive(false)
{
	CRY_TODO(01, 12, 2009, "Performance: consider using dynamic sized vector w/ reserve instead resize.");
	m_silhouettesVector.resize(32);
}

//-----------------------------------------------------------------------------------------------------

CHUDSilhouettes::~CHUDSilhouettes()
{
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::Clear()
{
	stl::free_container(m_silhouettesVector);
	m_silhouettesFGVector.clear();
}

void CHUDSilhouettes::Fill()
{
	m_silhouettesVector.resize(32);
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetVisionParams(EntityId uiEntityId,float r,float g,float b,float a)
{
	// When quick loading, entities may have been already destroyed when we do a ShowBinoculars(false)
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(uiEntityId);
	if(!pEntity)
		return;
	
	//BorisW: This will currently always and only render for the first RenderNode,
	//it might sense to provide slot selection as a parameter in HUDSilhouette's interface
	IRenderNode* pRenderNode = pEntity->GetRenderNode();
	if (pRenderNode)
	{
		pRenderNode->m_nHUDSilhouettesParam = CHUDUtils::ConverToSilhouetteParamValue(r, g, b, a);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetFlowGraphSilhouette(IEntity *pEntity,float r,float g,float b,float a,float fDuration)
{
	if(!pEntity)
		return;

	//Beni - Do not return, we might need to update the color, and SetShilhouette() will do the rest 
	//if(GetFGSilhouette(pEntity->GetId()) != m_silhouettesFGVector.end())
	//{
		//return;
	//}

	SetSilhouette(pEntity, r, g, b ,a, fDuration, true);
	m_silhouettesFGVector[pEntity->GetId()] = Vec3(r,g,b);
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::ResetFlowGraphSilhouette(EntityId uiEntityId)
{
	std::map<EntityId, Vec3>::iterator it = GetFGSilhouette(uiEntityId);
	if(it != m_silhouettesFGVector.end())
	{
		m_silhouettesFGVector.erase(it);
		ResetSilhouette(uiEntityId);
		return; 
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IEntity *pEntity,float r,float g,float b,float a,float fDuration, bool bFlowGraphRequested /*= false*/)
{
	if(!CanUseSilhouettes(bFlowGraphRequested))
		return;

	if(!pEntity)
		return;

	SSilhouette *pSilhouette = NULL;

	// First pass: is that Id already in a slot?
	for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
	{
		SSilhouette *pSil = &(*iter);
		if(pEntity->GetId() == pSil->uiEntityId)
		{
			pSilhouette = pSil;
			break;
		}
	}

	if(!pSilhouette)
	{
		// Second pass: try to find a free slot
		for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
		{
			SSilhouette *pSil = &(*iter);
			if(!pSil->bValid)
			{
				pSilhouette = pSil;
				break;
			}
		}
	}

	if(!pSilhouette && s_hasNotYetWarnedAboutVectorSize)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CHUDSilhouettes: Too many silhouettes active, vector size should be increased!");
		s_hasNotYetWarnedAboutVectorSize = false;
	}

	if(pSilhouette)
	{
		pSilhouette->uiEntityId	= pEntity->GetId();
		pSilhouette->fTime = fDuration;
		pSilhouette->bValid			= true;
		pSilhouette->r = r;
		pSilhouette->g = g;
		pSilhouette->b = b;
		pSilhouette->a = a;

		SetVisionParams(pEntity->GetId(),r,g,b,a);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IEntity *pEntity,	float fDuration, bool bFlowGraphRequested/*=false*/)
{
	const ColorF&  highlightCol = CHUDUtils::GetHUDColor();
	SetSilhouette(pEntity, highlightCol.r, highlightCol.g, highlightCol.b, highlightCol.a, fDuration, bFlowGraphRequested);
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IActor *pActor,float r,float g,float b,float a,float fDuration,bool bHighlightCurrentItem,bool bHighlightAccessories)
{
	if(!pActor || !pActor->GetEntity())
		return;

	SetSilhouette(pActor->GetEntity(),r,g,b,a,fDuration);

	if(bHighlightCurrentItem)
	{
		SetSilhouette(pActor->GetCurrentItem(),r,g,b,a,fDuration,bHighlightAccessories);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IActor *pActor,	float fDuration, bool bHighlightCurrentItem/*=true*/, bool bHighlightAccessories/*=true*/)
{
	const ColorF&  highlightCol = CHUDUtils::GetHUDColor();
	SetSilhouette(pActor, highlightCol.r, highlightCol.g, highlightCol.b, highlightCol.a, fDuration, bHighlightCurrentItem, bHighlightAccessories);
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IItem *pItem,float r,float g,float b,float a,float fDuration,bool bHighlightAccessories)
{
	if(!pItem)
		return;

	SetSilhouette(pItem->GetEntity(),r,g,b,a,fDuration);
	
	if(bHighlightAccessories)
	{
		const CItem::TAccessoryArray& accessories = static_cast<CItem*>(pItem)->GetAccessories();
		const int numAccessories = accessories.size();
		for(int i = 0; i < numAccessories; i++)
		{
			SetSilhouette(gEnv->pEntitySystem->GetEntity(accessories[i].accessoryId),r,g,b,a,fDuration);
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IItem	*pItem,	float fDuration, bool bHighlightAccessories/*=true*/)
{
	const ColorF&  highlightCol = CHUDUtils::GetHUDColor();
	SetSilhouette(pItem, highlightCol.r, highlightCol.g, highlightCol.b, highlightCol.a, fDuration, bHighlightAccessories);
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IVehicle *pVehicle,float r,float g,float b,float a,float fDuration)
{
	if(!pVehicle)
		return;

	SetSilhouette(pVehicle->GetEntity(),r,g,b,a,fDuration);
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IVehicle *pVehicle, float fDuration)
{
	const ColorF&  highlightCol = CHUDUtils::GetHUDColor();
	SetSilhouette(pVehicle, highlightCol.r, highlightCol.g, highlightCol.b, highlightCol.a, fDuration);
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::ResetSilhouette(EntityId uiEntityId)
{
	for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
	{
		SSilhouette *pSilhouette = &(*iter);

		if(pSilhouette->uiEntityId == uiEntityId && pSilhouette->bValid)
		{
			std::map<EntityId, Vec3>::iterator it = GetFGSilhouette(pSilhouette->uiEntityId);
			if(it != m_silhouettesFGVector.end())
			{
				Vec3 color = it->second;
				SetVisionParams(uiEntityId, color.x, color.y, color.z, 1.0f);
			}
			else
			{
				SetVisionParams(pSilhouette->uiEntityId,0,0,0,0);
				pSilhouette->bValid = false;
			}

			return;
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetType(int iType)
{
	// Exit of binoculars: we need to reset all silhouettes
	if(0 == iType)
	{
		for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
		{
			SSilhouette *pSilhouette = &(*iter);

			if(pSilhouette->bValid)
			{
				std::map<EntityId, Vec3>::iterator it = GetFGSilhouette(pSilhouette->uiEntityId);
				if(it != m_silhouettesFGVector.end())
				{
					Vec3 color = it->second;
					SetVisionParams(pSilhouette->uiEntityId, color.x, color.y, color.z, 1.0f);
				}
				else
				{
					SetVisionParams(pSilhouette->uiEntityId,0,0,0,0);
					pSilhouette->bValid = false;
				}
			}

		}
	}

	gEnv->p3DEngine->SetPostEffectParam("HudSilhouettes_Type",(float)(1-iType));
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::Update(float frameTime)
{
// "This might be reenabled if we want to have heat vision working proper with hud silhouettes"
 	bool currentHeatVisionMode = false; //g_pGame->IsHeatVisionOn();
 	if (currentHeatVisionMode && currentHeatVisionMode != m_heatVisionActive)
 		HeatVisionActivated();
 	m_heatVisionActive = currentHeatVisionMode;

	for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
	{
		SSilhouette *pSilhouette = &(*iter);

		if (pSilhouette->bValid)
		{
#ifndef _RELEASE
			if (g_pGameCVars->g_displayDbgText_silhouettes)
			{
				IEntity * e = gEnv->pEntitySystem->GetEntity(pSilhouette->uiEntityId);
				CryWatch("$8[SILHOUETTES]$o eID=%u %s '%s' time=%.2f col=[%.2f %.2f %.2f %.2f]", pSilhouette->uiEntityId, e ? e->GetClass()->GetName() : "NULL entity", e ? e->GetName() : "N/A", pSilhouette->fTime, pSilhouette->r, pSilhouette->g, pSilhouette->b, pSilhouette->a);
			}
#endif

			if (currentHeatVisionMode)
			{
				SetVisionParams(pSilhouette->uiEntityId, 0.f, 0.f, 0.f, 0.f);
			}
			else
			{
				SetVisionParams(pSilhouette->uiEntityId, pSilhouette->r, pSilhouette->g, pSilhouette->b, pSilhouette->a);
			}

			if(pSilhouette->fTime != -1)
			{
				pSilhouette->fTime -= frameTime;
				if(pSilhouette->fTime < 0.0f)
				{
					SetVisionParams(pSilhouette->uiEntityId,0,0,0,0);
					pSilhouette->bValid = false;
					pSilhouette->fTime = 0.0f;
				}
				else if (pSilhouette->fTime < 1.0f)
				{
					// fade out for the last second
					float scale = pSilhouette->fTime ;
					scale *= scale;
					SetVisionParams(pSilhouette->uiEntityId,pSilhouette->r*scale,pSilhouette->g*scale,pSilhouette->b*scale,pSilhouette->a*scale);
				}
			}
		}
	}

/*
	//Iterate again to render the debug info 
	//It is done like this, for efficiency using the UIDraw and AuxRenderGeometry (this should be redone completely anyways)
	IRenderAuxGeom* pAuxRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags oldFlags = pAuxRenderer->GetRenderFlags();
	SAuxGeomRenderFlags newFlags = e_Def3DPublicRenderflags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	pAuxRenderer->SetRenderFlags(newFlags);

	for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
	{
		SSilhouette *pSilhouette = &(*iter);

		if(pSilhouette->bValid && pSilhouette->fTime != -1)
		{
			//Debug infiltration mode extra info
			if(pSilhouette->debugInfiltrationInfo.bEnabled)
			{
				pSilhouette->debugInfiltrationInfo.fTime -= frameTime;
				if(pSilhouette->debugInfiltrationInfo.fTime <= 0.0f)
				{
					pSilhouette->debugInfiltrationInfo.bEnabled = false;
				}
				else
				{
					DrawDebugInfiltrationInfo(*pSilhouette);
				}
			}
			else if(pSilhouette->debugCombatInfo.bEnabled)
			{
				pSilhouette->debugCombatInfo.fTime -= frameTime;
				if(pSilhouette->debugCombatInfo.fTime <= 0.0f)
				{
					pSilhouette->debugCombatInfo.bEnabled = false;
				}
				else
				{
					DrawDebugCombatInfo(*pSilhouette);
				}
			}
		}
	}

	pAuxRenderer->SetRenderFlags(oldFlags);
*/
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::Serialize(TSerialize &ser)
{
	if(ser.IsReading())
		m_silhouettesFGVector.clear();

	int amount = m_silhouettesFGVector.size();
	ser.Value("amount", amount);
	if(amount)
	{
		EntityId id = 0;
		Vec3 color;
		if(ser.IsWriting())
		{
			std::map<EntityId, Vec3>::iterator it = m_silhouettesFGVector.begin();
			std::map<EntityId, Vec3>::iterator end = m_silhouettesFGVector.end();
			for(; it != end; ++it)
			{
				ser.BeginGroup("FlowGraphSilhouette");
				id = it->first;
				color = it->second;
				ser.Value("id", id);
				ser.Value("color", color);
				ser.EndGroup();
			}
		}
		else if(ser.IsReading())
		{
			for(int i = 0; i < amount; ++i)
			{
				ser.BeginGroup("FlowGraphSilhouette");
				ser.Value("id", id);
				ser.Value("color", color);
				ser.EndGroup();
				if(IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id))
					SetFlowGraphSilhouette(pEntity, color.x, color.y, color.z, 1.0f, -1.0f);
			}
		}
	}
}

//----------------------------------------------------
bool CHUDSilhouettes::CanUseSilhouettes(bool bFlowGraphRequested /*= false*/) const
{
	//Allow always from FlowGraph
	if(bFlowGraphRequested)
		return true;

	IActor* pActor = g_pGame->GetIGameFramework()->GetClientActor();
	return pActor != NULL;
}

//---------------------------------------------------------------------
void CHUDSilhouettes::SetSilhoutteInCombatMode(IEntity* pClientEntity, IEntity* pTargetEntity)
{
}

//------------------------------------------------------------------
void CHUDSilhouettes::SetSilhouetteInInfiltrationMode(IEntity* pClientEntity, IEntity* pTargetEntity)
{
	IAIObject* pAIObject = pTargetEntity->GetAI();
	
	if(pAIObject != NULL)
	{
		if(pAIObject->IsHostile(pClientEntity->GetAI(), false))
		{
			
			/*
			int targetAIGroup = pAIObject->GetGroupId();
			int	maxAlertness = 0;
			IAISystem* pAISystem = gEnv->pAISystem;
			
			int groupCount = pAISystem->GetGroupCount(targetAIGroup, IAISystem::GROUP_ENABLED);
			for (int i = 0; i < n; i++)
			{
				IAIObject*	pMember = pAISystem->GetGroupMember(groupID, i, IAISystem::GROUP_ENABLED);
				if (pMember && pMember->GetProxy())
					maxAlertness = max(maxAlertness, pMember->GetProxy()->GetAlertnessState());
			}

			//SAIDetectionLevels detectionLevels;
			//gEnv->pAISystem->GetDetectionLevels(0, detectionLevels);
			//float currentAwareness = max(detectionLevels.vehicleThreat, detectionLevels.puppetThreat);

			//SetSilhouette(pTargetEntity, currentAwareness,0,0,1.0f, 2.5f);

			*/

			//NOTE: Consider using awareness instead, or a combination of alertness and awareness
			//      to get the puttet state
			//      It seems that the IDLE state it doesn't match always the behaviour you see on screen
			const int alertness = pAIObject->GetProxy() ? pAIObject->GetProxy()->GetAlertnessState() : 0;

			//Always blue in infiltration mode
			SetSilhouette(pTargetEntity, 0.10588f, 0.10588f, 0.89411f, 1.0f, 2.5f, false);	

			SDebugInfiltrationInfo debugIcon;
			debugIcon.fTime = 2.0f;

			switch(alertness)
			{
				//case 0: //IDLE, NOT ALERTED
					//debugIconColor.Set(0.10588f, 0.89411f, 0.10588f);
					//break;
				
				case 1: //ALERTED
					debugIcon.bEnabled = true;
					debugIcon.r = 0.7411f; debugIcon.g = 0.7411f; debugIcon.b = 0.10588f; debugIcon.a = 1.0f;
					break;

				case 2: //COMBAT
					debugIcon.bEnabled = true;
					debugIcon.r = 0.89411f; debugIcon.g = 0.10588f; debugIcon.b = 0.10588f; debugIcon.a = 1.0f;
					break;
			}

			SetDebugInfiltrationInfo(pTargetEntity, debugIcon);
		}
	}
}

//-----------------------------------------------------
void CHUDSilhouettes::SetSilhouetteInTacticalMode(IEntity* pClientEntity, IEntity* pTargetEntity)
{
	IAIObject* pAIObject = pTargetEntity->GetAI();

	if (((pClientEntity->GetPos() - pTargetEntity->GetPos()).len2() < 5.0f) && (pTargetEntity->GetPhysics() == NULL))
		return;

	if(pAIObject != NULL)
	{
		if(pAIObject->IsHostile(pClientEntity->GetAI(), false))
			SetSilhouette(pTargetEntity, 0.89411f,0.89411f,0.10588f,1.0f, 2.5f);	
	}
		SetSilhouette(pTargetEntity, 0.89411f,0.89411f,0.10588f,1.0f, 2.5f);	
}

//------------------------------------------------------
void CHUDSilhouettes::SetDebugInfiltrationInfo(IEntity *pEntity, const SDebugInfiltrationInfo& debugIconInfo)
{
	if(!pEntity)
		return;

	SSilhouette *pSilhouette = NULL;

	// First pass: only if the silhoutte is there we can add the icon
	for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
	{
		SSilhouette *pSil = &(*iter);
		if(pEntity->GetId() == pSil->uiEntityId)
		{
			pSilhouette = pSil;
			break;
		}
	}

	//We have a silhoutte already, add the debug icon info
	if(pSilhouette)
	{
		pSilhouette->debugInfiltrationInfo = debugIconInfo;
	}
}

//------------------------------------------------------
void CHUDSilhouettes::SetDebugCombatInfo(IEntity *pEntity, const SDebugCombatInfo& debugCombatInfo)
{
	if(!pEntity)
		return;

	SSilhouette *pSilhouette = NULL;

	// First pass: only if the silhoutte is there we can add the icon
	for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
	{
		SSilhouette *pSil = &(*iter);
		if(pEntity->GetId() == pSil->uiEntityId)
		{
			pSilhouette = pSil;
			break;
		}
	}

	//We have a silhoutte already, add the debug icon info
	if(pSilhouette)
	{
		pSilhouette->debugCombatInfo = debugCombatInfo;
	}
}

//------------------------------------------------------
void CHUDSilhouettes::DrawDebugInfiltrationInfo(const SSilhouette& silhoutte)
{
	IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(silhoutte.uiEntityId);

	if(pTargetEntity)
	{
		const Vec3 conePos = pTargetEntity->GetPos() + (Vec3Constants<float>::fVec3_OneZ * 2.0f);
		const float coneRadius = 0.12f;
		const float coneHeight = 0.25f;

		ColorB coneColor;
		coneColor.r = (uint8) (silhoutte.debugInfiltrationInfo.r * 255.0f);
		coneColor.g = (uint8) (silhoutte.debugInfiltrationInfo.g * 255.0f);
		coneColor.b = (uint8) (silhoutte.debugInfiltrationInfo.b * 255.0f);
		coneColor.a = (silhoutte.debugInfiltrationInfo.fTime < 1.0f) ? (uint8) (silhoutte.debugInfiltrationInfo.fTime * 128) : 128;

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(conePos, -Vec3Constants<float>::fVec3_OneZ, coneRadius, coneHeight, coneColor);
	}
}

//------------------------------------------------------
void CHUDSilhouettes::DrawDebugCombatInfo(const SSilhouette& silhoutte)
{
	IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(silhoutte.uiEntityId);
	IActor* pClientActor = g_pGame->GetIGameFramework()->GetClientActor();

	const CActor* pCActor = static_cast<const CActor*>(pActor);

	if(pCActor != NULL && pClientActor != NULL)
	{
		Vec3 iconPos = pCActor->GetEntity()->GetPos() + (Vec3Constants<float>::fVec3_OneZ * 2.0f);

		ColorB color;
		color.r = 255;
		color.g = 0;
		color.b = 0;
		color.a = (silhoutte.debugCombatInfo.fTime < 1.0f) ? (uint8) (silhoutte.debugCombatInfo.fTime * 192) : 192;

		Vec3 playerToIconDir = iconPos - pClientActor->GetEntity()->GetPos();
		playerToIconDir.Normalize();

		if(playerToIconDir.Dot(pClientActor->GetEntity()->GetWorldTM().GetColumn1()) < 0.1f)
			return;

		//Draw health bar
		/*IUIDraw* pUIDraw = g_pGame->GetIGameFramework()->GetIUIDraw();

		if(pUIDraw)
		{
			float health = (float)pOldActor->GetHealth();
			float maxHelath = (float)pOldActor->GetMaxHealth();
			float healthFraction = CLAMP(health / maxHelath, 0.0f, 1.0f);

			const float barWidth = 35.0f;
			const float barHeight = 6.0f;

			Vec3 screenPos;
			gEnv->pRenderer->ProjectToScreen(iconPos.x, iconPos.y, iconPos.z - 0.25f, &screenPos.x, &screenPos.y, &screenPos.z);
			screenPos.x = screenPos.x / 100.0f * 800.0f;
			screenPos.y = screenPos.y / 100.0f * 600.0f;

			pUIDraw->PreRender();
			IRenderAuxImage::Draw2dImage(screenPos.x, screenPos.y, barWidth, barHeight, 0, 0, 0, 1, 1, 0, 0.5f, 0.5f, 0.5f, color.a);
			IRenderAuxImage::Draw2dImage(screenPos.x, screenPos.y, barWidth * healthFraction, barHeight, 0, 0, 0, 1, 1, 0, color.r, color.g, color.b, color.a, 0.9f);
			pUIDraw->PostRender();

		}*/

		//pAuxRenderer->DrawCone(conePos, -Vec3Constants<float>::fVec3_OneZ, coneRadius, coneHeight, coneColor);

	}
}

//-------------------------------------------------------------
std::map<EntityId, Vec3>::iterator CHUDSilhouettes::GetFGSilhouette(EntityId id)
{
	std::map<EntityId, Vec3>::iterator returnVal = m_silhouettesFGVector.end();

	std::map<EntityId, Vec3>::iterator it = m_silhouettesFGVector.begin();
	std::map<EntityId, Vec3>::iterator end = m_silhouettesFGVector.end();
	for(; it != end; ++it)
	{
		if(it->first == id)
			returnVal = it;
	}
	return returnVal;
}


//-------------------------------------------------------------
void CHUDSilhouettes::HeatVisionActivated()
{
	TSilhouettesVector::const_iterator begin = m_silhouettesVector.begin();
	TSilhouettesVector::const_iterator end = m_silhouettesVector.end();
	for(TSilhouettesVector::const_iterator iter = begin; iter != end; ++iter)
	{
		const SSilhouette& pSilhouette = *iter;

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pSilhouette.uiEntityId);
		if(!pEntity)
			continue;
		IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
		if(!pIEntityRender)
			continue;
		//pIEntityRender->SetVisionParams(0.0f, 0.0f, 0.0f, 0.0f);
	}
}
//-------------------------------------------------------------
void CHUDSilhouettes::CloakActivated( EntityId uiEntityId )
{
	if(gEnv->bMultiplayer)
	{
		TSilhouettesVector::iterator begin = m_silhouettesVector.begin();
		TSilhouettesVector::iterator end = m_silhouettesVector.end();
		for(TSilhouettesVector::iterator iter = begin; iter != end; ++iter)
		{
			if(iter->uiEntityId == uiEntityId)
			{
				// Disable silhouette as soon as entity cloaks
				SetVisionParams(iter->uiEntityId,0,0,0,0);
				iter->bValid = false;
				iter->fTime = 0.0f;
				return;
			}
		}
	}
}

