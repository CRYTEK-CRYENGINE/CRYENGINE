// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WaterRipplesGenerator.h"

#include <CryRenderer/IRenderAuxGeom.h>

#define WATER_RIPPLES_GENERATOR_UPDATE_SLOT 0

#if WATER_RIPPLES_EDITING_ENABLED
namespace
{
	bool TestLocation( const Vec3& testPosition )
	{
		const float threshold = 0.4f;
		const float waterLevel = gEnv->p3DEngine->GetWaterLevel( &testPosition );

		return (fabs_tpl(waterLevel - testPosition.z) < threshold);
	}
}
#endif

void CWaterRipplesGenerator::SProperties::InitFromScript( const IEntity& entity )
{
	IScriptTable* pScriptTable = entity.GetScriptTable();
	if (pScriptTable != NULL)
	{
		SmartScriptTable propertiesTable;
		if (pScriptTable->GetValue("Properties", propertiesTable))
		{
			propertiesTable->GetValue("fScale", m_scale);
			propertiesTable->GetValue("fStrength", m_strength);
			propertiesTable->GetValue("bEnabled", m_enabled);
			
			// Get the spawning parameters.
			SmartScriptTable spawnTable;
			if(propertiesTable->GetValue("Spawning", spawnTable))
			{
				spawnTable->GetValue("bAutoSpawn", m_autoSpawn);
				spawnTable->GetValue("bSpawnOnMovement", m_spawnOnMovement);
				spawnTable->GetValue("fFrequency", m_frequency);
			}

			// Get the randomizaiton parameters.
			SmartScriptTable randomTable;
			if(propertiesTable->GetValue("Randomization", randomTable))
			{
				randomTable->GetValue("fRandomFreq", m_randFrequency);
				randomTable->GetValue("fRandomScale", m_randScale);
				randomTable->GetValue("fRandomStrength", m_randStrength);
				randomTable->GetValue("fRandomOffsetX", m_randomOffset.x);
				randomTable->GetValue("fRandomOffsetY", m_randomOffset.y);
			}
		}
	}
}


namespace WRG
{
	void RegisterEvents( IGameObjectExtension& goExt, IGameObject& gameObject )
	{
		const int eventID = eGFE_ScriptEvent;
		gameObject.UnRegisterExtForEvents( &goExt, NULL, 0 );
		gameObject.RegisterExtForEvents( &goExt, &eventID, 1 );
	}
}

//////////////////////////////////////////////////////////////////////////
CWaterRipplesGenerator::CWaterRipplesGenerator()
	: m_lastSpawnTime(0)
{
#if WATER_RIPPLES_EDITING_ENABLED
	m_currentLocationOk = true;
#endif
}

CWaterRipplesGenerator::~CWaterRipplesGenerator()
{

}

bool CWaterRipplesGenerator::Init(IGameObject *pGameObject)
{
	SetGameObject(pGameObject);

	return true;
}

void CWaterRipplesGenerator::PostInit(IGameObject *pGameObject)
{
	Reset();

	WRG::RegisterEvents( *this, *pGameObject );
}

bool CWaterRipplesGenerator::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();

	WRG::RegisterEvents( *this, *pGameObject );

	CRY_ASSERT_MESSAGE(false, "CWaterRipplesGenerator::ReloadExtension not implemented");

	return false;
}

bool CWaterRipplesGenerator::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CWaterRipplesGenerator::GetEntityPoolSignature not implemented");

	return true;
}

void CWaterRipplesGenerator::Release()
{
	delete this;
}

void CWaterRipplesGenerator::FullSerialize(TSerialize ser)
{

}

void CWaterRipplesGenerator::Update(SEntityUpdateContext &ctx, int updateSlot)
{
	CRY_ASSERT( updateSlot == WATER_RIPPLES_GENERATOR_UPDATE_SLOT );

	float fFrequency = m_properties.m_frequency + cry_random(-1.0f, 1.0f)*m_properties.m_randFrequency;
	bool allowHit = (gEnv->pTimer->GetCurrTime() - m_lastSpawnTime) > fFrequency;
	if(m_properties.m_autoSpawn && allowHit)
		ProcessHit(false);

#if WATER_RIPPLES_EDITING_ENABLED
	const bool debugDraw = m_properties.m_enabled && gEnv->IsEditing() && g_pGame->DisplayEditorHelpersEnabled();
	if (debugDraw)
	{
		IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();

		const ColorF colorGoodPosition0(0.0f, 0.0f, 0.5f, 0.0f);
		const ColorF colorGoodPosition1(0.0f, 0.5f, 1.0f, 1.0f);
		const ColorF colorBadPosition0(1.0f, 0.1f, 0.1f, 0.0f);
		const ColorF colorBadPosition1(1.0f, 0.1f, 0.1f, 1.0f);

		const ColorF& surfaceColor0 = m_currentLocationOk ? colorGoodPosition0 : colorBadPosition0;
		const ColorF& surfaceColor1 = m_currentLocationOk ? colorGoodPosition1 : colorBadPosition1;

		const Vec3 surfacePosition(GetEntity()->GetWorldPos());

		pRenderAux->DrawSphere(surfacePosition, 0.1f, surfaceColor1);

		const static int lines = 8;
		const static float radius0 = 0.5f;
		const static float radius1 = 1.0f;
		const static float radius2 = 2.0f;
		for (int i = 0; i < lines; ++i)
		{
			const float radians = ((float)i / (float)lines) * gf_PI2;
			const float cosine = cos_tpl(radians);
			const float sinus = sin_tpl(radians);
			const Vec3 offset0(radius0 * cosine, radius0 * sinus, 0);
			const Vec3 offset1(radius1 * cosine, radius1 * sinus, 0);
			const Vec3 offset2(radius2 * cosine, radius2 * sinus, 0);
			pRenderAux->DrawLine(surfacePosition+offset0, surfaceColor0, surfacePosition+offset1, surfaceColor1, 2.0f);
			pRenderAux->DrawLine(surfacePosition+offset1, surfaceColor1, surfacePosition+offset2, surfaceColor0, 2.0f);
		}
	}
#endif
}

void CWaterRipplesGenerator::HandleEvent( const SGameObjectEvent &gameObjectEvent )
{
	if ((gameObjectEvent.event == eGFE_ScriptEvent) && (gameObjectEvent.param != NULL))
	{
		const char* eventName = static_cast<const char*>(gameObjectEvent.param);
		if (strcmp(eventName, "enable") == 0)
		{
			m_properties.m_enabled = true;
			ActivateGeneration( true );
		}
		else if (strcmp(eventName, "disable") == 0)
		{
			m_properties.m_enabled = false;
			ActivateGeneration( false );
		}
		else if (gEnv->IsEditor() && (strcmp(eventName, "propertyChanged") == 0))
		{
			Reset();
		}
	}
}

void CWaterRipplesGenerator::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_XFORM)
	{
		if(m_properties.m_spawnOnMovement)
			ProcessHit(true);

#if WATER_RIPPLES_EDITING_ENABLED
		if (gEnv->IsEditor())
		{
			m_currentLocationOk = TestLocation( GetEntity()->GetWorldPos() );
		}
#endif
	}
	else if (gEnv->IsEditor() && (event.event == ENTITY_EVENT_RESET))
	{
		const bool leavingGameMode = (event.nParam[0] == 0);
		if (leavingGameMode)
		{
			Reset();
		}
	}
}

uint64 CWaterRipplesGenerator::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM) | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET);
}

void CWaterRipplesGenerator::ProcessHit(bool isMoving)
{
	if ((m_properties.m_enabled))
	{
		Vec3 vWorldPos = GetEntity()->GetWorldPos();
		if(!isMoving) // No random offsets during movement spawning.
		{
			vWorldPos += Vec3(m_properties.m_randomOffset).CompMul(cry_random_componentwise(Vec3(-1, -1, 0), Vec3(1, 1, 0)));
		}

		float fScale = m_properties.m_scale + cry_random(-1.0f, 1.0f) * m_properties.m_randScale;
		float fStrength = m_properties.m_strength + cry_random(-1.0f, 1.0f) * m_properties.m_randStrength;

		if (gEnv->p3DEngine)
		{
			gEnv->p3DEngine->AddWaterRipple(vWorldPos, fScale, fStrength);
		}

		float fTime = gEnv->pTimer->GetCurrTime();
		m_lastSpawnTime = fTime;
	}
}

void CWaterRipplesGenerator::Reset()
{
	m_properties.InitFromScript( *GetEntity() );
	
	ActivateGeneration( m_properties.m_enabled );

#if WATER_RIPPLES_EDITING_ENABLED
	m_currentLocationOk = TestLocation( GetEntity()->GetWorldPos() );
#endif

}

void CWaterRipplesGenerator::ActivateGeneration( const bool activate )
{
	if (activate && (gEnv->IsEditor() || m_properties.m_autoSpawn))
	{
		if (GetGameObject()->GetUpdateSlotEnables( this, WATER_RIPPLES_GENERATOR_UPDATE_SLOT) == 0)
		{
			GetGameObject()->EnableUpdateSlot( this, WATER_RIPPLES_GENERATOR_UPDATE_SLOT ) ;
		}
	}
	else
	{
		GetGameObject()->DisableUpdateSlot( this, WATER_RIPPLES_GENERATOR_UPDATE_SLOT );
	}
}