// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIHUD3D.cpp
//  Version:     v1.00
//  Created:     22/11/2011 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "UIHUD3D.h"

#include <CryEntitySystem/IEntitySystem.h>
#include <Cry3DEngine/IMaterial.h>

#include "UIManager.h"
#include "UI/HUD/HUDEventDispatcher.h"

#include "Game.h"
#include "GameCVars.h"
#include "Actor.h"

#include <CryGame/IGameFramework.h>
#include <IGameRulesSystem.h>
#include "Player.h"

#define HUD3D_PREFAB_LIB  "Prefabs/HUD.xml"
#define HUD3D_PREFAB_NAME "HUD.HUD_3D"

////////////////////////////////////////////////////////////////////////////
CUIHUD3D::CUIHUD3D()
	: m_pHUDRootEntity(NULL)
	, m_fHudDist(1.1f)
	, m_fHudZOffset(-0.1f)
	, m_HUDRootEntityId(0)
{

}

CUIHUD3D::~CUIHUD3D()
{
	RemoveHudEntities();
}

////////////////////////////////////////////////////////////////////////////
void CUIHUD3D::InitEventSystem()
{
	assert(gEnv->pSystem);
	if (gEnv->pSystem)
		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CUIHUD3D");

	if (gEnv->pFlashUI)
		gEnv->pFlashUI->RegisterModule(this, "CUIHUD3D");

	m_Offsets.push_back(SHudOffset(1.2f, 1.4f, -0.2f));
	m_Offsets.push_back(SHudOffset(1.33f, 1.3f, -0.1f));
	m_Offsets.push_back(SHudOffset(1.6f, 1.1f, -0.1f));
	m_Offsets.push_back(SHudOffset(1.77f, 1.0f, 0.0f));

	ICVar* pShowHudVar = gEnv->pConsole->GetCVar("hud_hide");
	if (pShowHudVar)
		pShowHudVar->SetOnChangeCallback(&OnVisCVarChange);
}

////////////////////////////////////////////////////////////////////////////
void CUIHUD3D::UnloadEventSystem()
{
	if (gEnv->pSystem)
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
		gEnv->pEntitySystem->RemoveEntityEventListener(m_HUDRootEntityId, ENTITY_EVENT_DONE, this);
	}

	if (gEnv->pFlashUI)
		gEnv->pFlashUI->UnregisterModule(this);
}

////////////////////////////////////////////////////////////////////////////
void CUIHUD3D::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_GAMEPLAY_START:
		SpawnHudEntities();
		break;
	case ESYSTEM_EVENT_LEVEL_LOAD_RESUME_GAME:
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		RemoveHudEntities();
		break;
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIHUD3D::OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_DONE)
	{
		/* NOTE: atm it's not safe to remove the listener here already */
		//gEnv->pEntitySystem->RemoveEntityEventListener(pEntity->GetId(), ENTITY_EVENT_DONE, this);

		m_pHUDRootEntity = NULL;
		m_HUDRootEntityId = 0;
		m_HUDEnties.clear();
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIHUD3D::UpdateView(const SViewParams& viewParams)
{
	CActor* pLocalPlayer = (CActor*)g_pGame->GetIGameFramework()->GetClientActor();

	if (gEnv->IsEditor() && !gEnv->IsEditing() && !m_pHUDRootEntity)
		SpawnHudEntities();

	// When you die we destroy the HUD, so this will make sure it re-spawns when you respawned
	if (!gEnv->IsEditor() && !m_pHUDRootEntity)
	{
		// ignore invalid file access when reloading HUD after dying
		// this is not ideal - better try to load all resources in the beginning
		SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

		SpawnHudEntities();
	}

	const CUICVars* pCVars = g_pGame->GetUI()->GetCVars();
	if (m_pHUDRootEntity && pLocalPlayer)
	{
		if (pCVars->hud_detach)
			return;

		const QuatT& cameraTran = pLocalPlayer->GetCameraTran();
		const QuatT& hudTran = pLocalPlayer->GetHUDTran();

		const Quat& cameraRotation = cameraTran.q;
		const Quat& hudRotation = hudTran.q;
		Quat deltaHudRotation = cameraRotation.GetInverted() * hudRotation;

		//If third person is active don't use the deltaHudRotation to avoid strange HUD behavior
		if (pLocalPlayer->IsThirdPerson())
			deltaHudRotation.SetIdentity();

		if (pCVars->hud_bobHud > 0.0f && !pLocalPlayer->IsDead())
		{
			deltaHudRotation.w *= (float)__fres(pCVars->hud_bobHud);

			// Avoid divide by 0
			if (!(fabs(deltaHudRotation.w) < FLT_EPSILON)) // IsValid() doesn't catch it
			{
				deltaHudRotation.Normalize();
			}
		}
		else
		{
			deltaHudRotation.SetIdentity();
		}

		// In general use the player position + viewparams override
		Vec3 viewPosition = pLocalPlayer->GetEntity()->GetPos() + viewParams.position;
		Quat clientRotation = viewParams.rotation * deltaHudRotation;

		// Override special cases: Third person should use camera-oriented HUD instead of HUD-bone oriented
		// Sliding should use the Bone instead of the viewParams solution
		if (pLocalPlayer->IsThirdPerson() || pLocalPlayer->GetLinkedVehicle() != NULL)
		{
			viewPosition = viewParams.position;
		}
		else
		{
			CPlayer* player = (CPlayer*)pLocalPlayer;
			if (player && player->IsSliding())
			{
				viewPosition = pLocalPlayer->GetEntity()->GetWorldTM().TransformPoint(hudTran.t);
			}
		}
		//

		const Vec3 forward(clientRotation.GetColumn1());
		const Vec3 up(clientRotation.GetColumn2());
		const Vec3 right(-(up % forward));

		const float distance = pCVars->hud_cameraOverride ? pCVars->hud_cameraDistance : m_fHudDist;
		const float offset = pCVars->hud_cameraOverride ? pCVars->hud_cameraOffsetZ : m_fHudZOffset;

		float offsetScale = 1.0f;
		if (viewParams.fov > 0.0f)
		{
			offsetScale = (pCVars->hud_cgf_positionScaleFOV * __fres(viewParams.fov)) + distance;
		}

		// Allow overscanBorders to control safe zones
		Vec2 overscanBorders = ZERO;
		if (gEnv->pRenderer)
		{
			gEnv->pRenderer->EF_Query(EFQ_OverscanBorders, overscanBorders);
		}
		const float viewDepth = 1.0f + (pCVars->hud_overscanBorder_depthScale * overscanBorders.y);
		viewPosition += forward * viewDepth * offsetScale;

		viewPosition += (up * offset);
		viewPosition += (right * pCVars->hud_cgf_positionRightScale);
		const Vec3 posVec(viewPosition + (forward * 0.001f));

		static const Quat rot90Deg = Quat::CreateRotationXYZ(Ang3(gf_PI * 0.5f, 0, 0));
		const Quat rotation = clientRotation * rot90Deg; // rotate 90 degrees around X-Axis

		m_pHUDRootEntity->SetPosRotScale(posVec, rotation, m_pHUDRootEntity->GetScale());
	}

	if (gEnv->pRenderer && m_pHUDRootEntity && pCVars->hud_debug3dpos > 0)
	{
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_pHUDRootEntity->GetWorldPos(), 0.2f, ColorB(255, 0, 0));
		const int children = m_pHUDRootEntity->GetChildCount();
		for (int i = 0; i < children && pCVars->hud_debug3dpos > 1; ++i)
		{
			IEntity* pChild = m_pHUDRootEntity->GetChild(i);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(pChild->GetWorldPos(), 0.1f, ColorB(255, 255, 0));
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIHUD3D::Reload()
{
	if (gEnv->IsEditor())
	{
		RemoveHudEntities();
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIHUD3D::Update(float fDeltaTime)
{
	static int width = -1;
	static int height = -1;
	int currWidth = 1;
	int currHeight = 1;

	CActor* pLocalPlayer = (CActor*)g_pGame->GetIGameFramework()->GetClientActor();
	bool bVisible = g_pGame->GetUI()->GetCVars()->hud_hide == 0;
	SetVisible(bVisible && pLocalPlayer && !pLocalPlayer->IsDead());

	if (gEnv->IsEditor())
	{
		currWidth = gEnv->pRenderer->GetOverlayWidth();
		currHeight = gEnv->pRenderer->GetOverlayHeight();
	}
	else
	{
		static ICVar* pCVarWidth = gEnv->pConsole->GetCVar("r_Width");
		static ICVar* pCVarHeight = gEnv->pConsole->GetCVar("r_Height");
		currWidth = pCVarWidth ? pCVarWidth->GetIVal() : 1;
		currHeight = pCVarHeight ? pCVarHeight->GetIVal() : 1;

		CRY_ASSERT(currWidth == gEnv->pRenderer->GetOverlayWidth());
		CRY_ASSERT(currHeight == gEnv->pRenderer->GetOverlayHeight());

		__debugbreak();
	}

	if (currWidth != width || currHeight != height)
	{
		width = currWidth;
		height = currHeight;
		float aspect = (float) width / (float) height;
		THudOffset::const_iterator foundit = m_Offsets.end();
		for (THudOffset::const_iterator it = m_Offsets.begin(); it != m_Offsets.end(); ++it)
		{
			if (foundit == m_Offsets.end() || fabs(it->Aspect - aspect) < fabs(foundit->Aspect - aspect))
				foundit = it;
		}
		if (foundit != m_Offsets.end())
		{
			m_fHudDist = foundit->HudDist;
			m_fHudZOffset = foundit->HudZOffset;
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIHUD3D::SpawnHudEntities()
{
	RemoveHudEntities();

	if (gEnv->IsEditor() && gEnv->IsEditing())
		return;

	const char* hudprefab = NULL;
	IGameRules* pGameRules = gEnv->pGameFramework->GetIGameRulesSystem()->GetCurrentGameRules();
	if (pGameRules)
	{
		IScriptTable* pTable = pGameRules->GetEntity()->GetScriptTable();
		if (pTable)
		{
			if (!pTable->GetValue("hud_prefab", hudprefab))
				hudprefab = NULL;
		}
	}
	hudprefab = hudprefab ? hudprefab : HUD3D_PREFAB_LIB;

	XmlNodeRef node = gEnv->pSystem->LoadXmlFromFile(hudprefab);
	if (node)
	{
		// get the prefab with the name defined in HUD3D_PREFAB_NAME
		XmlNodeRef prefab = NULL;
		for (int i = 0; i < node->getChildCount(); ++i)
		{
			const char* name = node->getChild(i)->getAttr("Name");
			if (name && strcmp(name, HUD3D_PREFAB_NAME) == 0)
			{
				prefab = node->getChild(i);
				prefab = prefab ? prefab->findChild("Objects") : XmlNodeRef();
				break;
			}
		}

		if (prefab)
		{
			// get the PIVOT entity and collect childs
			XmlNodeRef pivotNode = NULL;
			std::vector<XmlNodeRef> childs;
			const int count = prefab->getChildCount();
			childs.reserve(count - 1);

			for (int i = 0; i < count; ++i)
			{
				const char* name = prefab->getChild(i)->getAttr("Name");
				if (strcmp("PIVOT", name) == 0)
				{
					assert(pivotNode == NULL);
					pivotNode = prefab->getChild(i);
				}
				else
				{
					childs.push_back(prefab->getChild(i));
				}
			}

			if (pivotNode)
			{
				// spawn pivot entity
				IEntityClass* pEntClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pivotNode->getAttr("EntityClass"));
				if (pEntClass)
				{
					SEntitySpawnParams params;
					params.nFlags = ENTITY_FLAG_CLIENT_ONLY;
					params.pClass = pEntClass;
					m_pHUDRootEntity = gEnv->pEntitySystem->SpawnEntity(params);
				}

				if (!m_pHUDRootEntity) return;

				m_HUDRootEntityId = m_pHUDRootEntity->GetId();
				gEnv->pEntitySystem->AddEntityEventListener(m_HUDRootEntityId, ENTITY_EVENT_DONE, this);

				// spawn the childs and link to the pivot enity
				for (std::vector<XmlNodeRef>::iterator it = childs.begin(); it != childs.end(); ++it)
				{
					XmlNodeRef child = *it;
					pEntClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(child->getAttr("EntityClass"));
					if (pEntClass)
					{
						const char* material = child->getAttr("Material");
						Vec3 pos;
						Vec3 scale;
						Quat rot;
						child->getAttr("Pos", pos);
						child->getAttr("Rotate", rot);
						child->getAttr("Scale", scale);

						SEntitySpawnParams params;
						params.nFlags = ENTITY_FLAG_CLIENT_ONLY;
						params.pClass = pEntClass;
						params.vPosition = pos;
						params.qRotation = rot;
						params.vScale = scale;
						IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(params);
						if (pEntity)
						{
							IScriptTable* pScriptTable = pEntity->GetScriptTable();
							if (pScriptTable)
							{
								SmartScriptTable probs;
								pScriptTable->GetValue("Properties", probs);

								XmlNodeRef properties = child->findChild("Properties");
								if (probs && properties)
								{
									for (int k = 0; k < properties->getNumAttributes(); ++k)
									{
										const char* sKey;
										const char* sVal;
										properties->getAttributeByIndex(k, &sKey, &sVal);
										probs->SetValue(sKey, sVal);
									}
								}
								if (pScriptTable->HaveValue("OnPropertyChange"))
									Script::CallMethod(pScriptTable, "OnPropertyChange");
							}

							if (material)
							{
								IMaterial* pMat = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(material);
								if (pMat)
									pEntity->SetMaterial(pMat);
							}
							m_pHUDRootEntity->AttachChild(pEntity);
							m_HUDEnties.push_back(pEntity->GetId());
						}
					}
				}
			}
		}
	}

	// Register as ViewSystem listener (for cut-scenes, ...)
	IViewSystem* pViewSystem = g_pGame->GetIGameFramework()->GetIViewSystem();
	if (pViewSystem)
	{
		pViewSystem->AddListener(this);
	}

	OnVisCVarChange(NULL);

	CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnHUDLoadDone));
}

////////////////////////////////////////////////////////////////////////////
void CUIHUD3D::RemoveHudEntities()
{
	// Unregister as ViewSystem listener (for cut-scenes, ...)
	IViewSystem* pViewSystem = g_pGame->GetIGameFramework()->GetIViewSystem();
	if (pViewSystem)
	{
		pViewSystem->RemoveListener(this);
	}

	if (m_HUDRootEntityId)
		gEnv->pEntitySystem->RemoveEntity(m_HUDRootEntityId, true);
	for (THUDEntityList::const_iterator it = m_HUDEnties.begin(); it != m_HUDEnties.end(); ++it)
		gEnv->pEntitySystem->RemoveEntity(*it, true);

	m_pHUDRootEntity = NULL;
	m_HUDRootEntityId = 0;
	m_HUDEnties.clear();
}

////////////////////////////////////////////////////////////////////////////
void CUIHUD3D::SetVisible(bool visible)
{
	IEntity* pHUDRootEntity = gEnv->pEntitySystem->GetEntity(m_HUDRootEntityId);
	if (pHUDRootEntity)
		pHUDRootEntity->Invisible(!visible);
	for (THUDEntityList::const_iterator it = m_HUDEnties.begin(); it != m_HUDEnties.end(); ++it)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*it);
		if (pEntity)
			pEntity->Invisible(!visible);
	}
}

////////////////////////////////////////////////////////////////////////////
bool CUIHUD3D::IsVisible() const
{
	return m_pHUDRootEntity ? !m_pHUDRootEntity->IsInvisible() : false;
}

////////////////////////////////////////////////////////////////////////////
void CUIHUD3D::OnVisCVarChange(ICVar*)
{
	if (!g_pGame || !g_pGame->GetUI() || !g_pGame->GetUI()->GetCVars())
		return;

	bool bVisible = (g_pGame->GetUI()->GetCVars()->hud_hide == 0);

	CUIHUD3D* pHud3D = UIEvents::Get<CUIHUD3D>();
	if (pHud3D)
	{
		pHud3D->SetVisible(bVisible);
	}

	if (gEnv->pFlashUI)
		gEnv->pFlashUI->SetHudElementsVisible(bVisible);
}

////////////////////////////////////////////////////////////////////////////
bool CUIHUD3D::OnCameraChange(const SCameraParams& cameraParams)
{
	IViewSystem* pViewSystem = g_pGame->GetIGameFramework()->GetIViewSystem();
	if (pViewSystem)
	{
		if (gEnv->pFlashUI)
		{
			gEnv->pFlashUI->SetHudElementsVisible(pViewSystem->IsClientActorViewActive());
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////
REGISTER_UI_EVENTSYSTEM(CUIHUD3D);
