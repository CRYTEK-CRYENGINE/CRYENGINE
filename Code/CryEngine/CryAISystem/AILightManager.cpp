// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AILightManager.cpp
   $Id$
   Description: Keeps track of the logical light level in the game world and
   provides services to make light levels queries.

   -------------------------------------------------------------------------
   History:
   - 2007				: Created by Mikko Mononen

 *********************************************************************/

#include "StdAfx.h"
#include "AILightManager.h"
#include "DebugDrawContext.h"
#include "AIPlayer.h"
#include <Cry3DEngine/ITimeOfDay.h>

static char g_szLightLevels[][10] =
{
	"Default",
	"Light",
	"Medium",
	"Dark",
	"SuperDark"
};

static char g_szLightType[][16] =
{
	"Generic",
	"Muzzle Flash",
	"Flash Light",
	"Laser",
};

//===================================================================
// CAILightManager
//===================================================================
CAILightManager::CAILightManager() :
	m_ambientLight(AILL_LIGHT),
	m_updated(false)
{
}

//===================================================================
// Reset
//===================================================================
void CAILightManager::Reset()
{
	CCCPOINT(CAILightManager_Reset);

	m_ambientLight = AILL_LIGHT;

	// (MATT) This is all that's needed, strong refs (refAttrib?) should give up ownership, weak don't care  {2009/02/15}
	stl::free_container(m_dynLights);
	stl::free_container(m_lights);
}

//===================================================================
// DynOmniLightEvent
//===================================================================
void CAILightManager::DynOmniLightEvent(const Vec3& pos, float radius, EAILightEventType type, CAIActor* pShooter, float time)
{
	CCCPOINT(CAILightManager_DynOmniLightEvent);

	// (MATT) Are NULL shooters allowed? {2009/02/15}
	if (!pShooter)
	{
		assert(false);
		gEnv->pScriptSystem->RaiseError("[Not scripts!] CAILightManager_DynOmniLightEvent - NULL pShooter");
	}
	// (MATT) This is a search function, hence does not care if some references are invalid {2009/02/15}
	CWeakRef<CAIActor> refShooter = GetWeakRef(pShooter);

	for (unsigned i = 0, ni = m_dynLights.size(); i < ni; ++i)
	{
		SAIDynLightSource& light = m_dynLights[i];
		if (refShooter == light.refShooter && light.fov < 0.0f)
		{
			light.pos = pos;
			light.radius = radius;
			light.type = type;
			light.t = 0;
			return;
		}
	}
	m_dynLights.push_back(SAIDynLightSource(pos, Vec3Constants<float>::fVec3_Zero, radius, -1.0f, AILL_LIGHT, type, refShooter, NILREF, time));
}

//===================================================================
// DynSpotLightEvent
//===================================================================
void CAILightManager::DynSpotLightEvent(const Vec3& pos, const Vec3& dir, float radius, float fov, EAILightEventType type, CAIActor* pShooter, float time)
{
	if (fov <= 0.0f)
		return;

	CCCPOINT(CAILightManager_DynSpotLightEvent);

	// (MATT) Are NULL shooters allowed? {2009/02/15}
	assert(pShooter);
	// (MATT) This is a search function, hence does not care if some references are invalid {2009/02/15}
	CWeakRef<CAIActor> refShooter = GetWeakRef(pShooter);

	for (unsigned i = 0, ni = m_dynLights.size(); i < ni; ++i)
	{
		SAIDynLightSource& light = m_dynLights[i];
		if (refShooter == light.refShooter && light.fov > 0)
		{
			CCCPOINT(CAILightManager_DynSpotLightEvent_A);

			light.pos = pos;
			light.dir = dir;
			light.radius = radius;
			light.fov = fov;
			light.t = 0;

			// Remember refAttrib is strong
			if (CAIObject* const pAttrib = light.refAttrib.GetAIObject())
			{
				if (type == AILE_FLASH_LIGHT)
					pAttrib->SetPos(pos + dir * radius / 2);
				else
					pAttrib->SetPos(pos + dir * radius);
			}
			return;
		}
	}

	// (MATT) Couldn't find an existing slot - create new {2009/02/15}
	CStrongRef<CAIObject> refAttrib;
	if (type == AILE_LASER || type == AILE_FLASH_LIGHT)
	{
		if (pShooter->GetType() == AIOBJECT_PLAYER)
		{
			CCCPOINT(CAILightManager_DynSpotLightEvent_B);

			// Since checking the attributes is really expensive, create them only for the player.

			// (MATT) We used to create an attribute with CreateAIObject. CreateDummy seems more appropriate and avoids push to m_Objects list {2009/03/30}
			static int nameCounter = 0;
			char szName[256];
			cry_sprintf(szName, "Light of %s %d", pShooter->GetName(), nameCounter++);
			gAIEnv.pAIObjectManager->CreateDummyObject(refAttrib, szName);
			CAIObject* pAttrib = refAttrib.GetAIObject();
			pAttrib->SetType(AIOBJECT_ATTRIBUTE);
			pAttrib->SetAssociation(refShooter);
		}

	}
	else
	{
		CCCPOINT(CAILightManager_DynSpotLightEvent_C);
	}

	m_dynLights.push_back(SAIDynLightSource(pos, dir, radius, fov, AILL_LIGHT, type, refShooter, refAttrib, time));
}

//===================================================================
// Update
//===================================================================
void CAILightManager::Update(bool forceUpdate)
{
	CCCPOINT(CAILightManager_Update);

	CTimeValue t = GetAISystem()->GetFrameStartTime();
	int64 dt = (t - m_lastUpdateTime).GetMilliSecondsAsInt64();
	int nDynLightsAtStart = m_dynLights.size();     // Just for debugging - the vector can shrink

	//	if (dt > 0.25f || forceUpdate)
	{
		m_lastUpdateTime = t;

		// Decay dyn lights.
		// (MATT) Already iterating and removing and done regularly - so this is a good place to erase elements with removed owners
		// Only fly in the ointment is that the pAttrib object must be looked up in ObjectContainer (but not accessed) {2009/02/15}
		for (unsigned i = 0; i < m_dynLights.size(); )
		{
			SAIDynLightSource& light = m_dynLights[i];
			light.t += (int)dt;

			// (MATT) Some extra CCCPoints here for debugging, not needed long-term {2009/02/16}
			if (light.refShooter.IsNil()) // I'm still not sure if a shooter is required for all entries
			{
				assert(false);
				CCCPOINT(CAILightManager_Update_NilShooter);
			}

			bool bValidShooter = light.refShooter.IsValid();
			if (!bValidShooter)
			{ CCCPOINT(CAILightManager_Update_NonValidShooter); }

			if (!bValidShooter || light.t > light.tmax)
			{
				// refAttrib freed automatically
				m_dynLights[i] = m_dynLights.back();
				m_dynLights.pop_back();
			}
			else
				++i;
		}

		UpdateTimeOfDay();
		UpdateLights();

		m_updated = true;
	}

}

//===================================================================
// DebugDraw
//===================================================================
void CAILightManager::DebugDraw()
{
	int mode = gAIEnv.CVars.DebugDrawLightLevel;
	if (mode == 0)
		return;

	static CTimeValue lastTime(0.0f);
	CTimeValue t = GetAISystem()->GetFrameStartTime();
	float dt = (t - lastTime).GetSeconds();
	if (!m_updated && dt < 0.0001f)
		Update(true);

	m_updated = false;
	lastTime = t;

	ColorB white(255, 255, 255);
	CDebugDrawContext dc;
	float height = static_cast<float>(dc->GetHeight());
	dc->Draw2dLabel(20.f, height - 100, 2.0f, white, false, "Ambient: %s", g_szLightLevels[(int)m_ambientLight]);
	dc->Draw2dLabel(20.f, height - 75, 1.5f, white, false, "Ent Lights: %" PRISIZE_T, m_lights.size());

	CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetAISystem()->GetPlayer());
	if (pPlayer)
	{
		ColorB playerEffectCol(white);
		string sPlayerEffect;
		if (pPlayer->IsAffectedByLight())
		{
			sPlayerEffect = "Player Light Level: ";
			const EAILightLevel lightLevel = pPlayer->GetLightLevel();
			switch (lightLevel)
			{
			case AILL_NONE:
				sPlayerEffect += "None";
				playerEffectCol = ColorB(128, 128, 128);
				break;
			case AILL_LIGHT:
				sPlayerEffect += "Light";
				playerEffectCol = ColorB(0, 255, 0);
				break;
			case AILL_MEDIUM:
				sPlayerEffect += "Medium";
				playerEffectCol = ColorB(255, 255, 0);
				break;
			case AILL_DARK:
				sPlayerEffect += "Dark";
				playerEffectCol = ColorB(255, 0, 0);
				break;
			case AILL_SUPERDARK:
				sPlayerEffect += "SuperDark";
				playerEffectCol = ColorB(255, 128, 255);
				break;
			default:
				CRY_ASSERT_MESSAGE(false, "CAILightManager::DebugDraw Unhandled light level");
				break;
			}
		}
		else
		{
			sPlayerEffect = "Player Not Affected by Light";
		}

		dc->Draw2dLabel(20.f, height - 50, 1.5f, playerEffectCol, false, "%s", sPlayerEffect.c_str());
	}

	if (mode == 2)
	{
		// Draw the player pos too.
		if (pPlayer)
		{
			Vec3 pos = pPlayer->GetPos();
			if (pos.IsZero())
				pos = dc->GetCameraPos();
			EAILightLevel playerLightLevel = GetLightLevelAt(pos);
			dc->Draw2dLabel(20.f, height - 35.f, 1.5f, white, false, "Player: %s", g_szLightLevels[(int)playerLightLevel]);
			dc->DrawLine(pos - Vec3(10, 0, 0), white, pos + Vec3(10, 0, 0), white);
			dc->DrawLine(pos - Vec3(0, 10, 0), white, pos + Vec3(0, 10, 0), white);
			dc->DrawLine(pos - Vec3(0, 0, 10), white, pos + Vec3(0, 0, 10), white);
		}
	}

	ColorB lightCols[4] = { ColorB(0, 0, 0), ColorB(255, 255, 255), ColorB(255, 196, 32), ColorB(16, 96, 128) };

	// Draw light bases and connections.
	for (unsigned i = 0, ni = m_lights.size(); i < ni; ++i)
	{
		SAILightSource& light = m_lights[i];
		ColorB c = lightCols[(int)light.level];
		dc->DrawSphere(light.pos, 0.2f, c);
	}

	for (unsigned i = 0, ni = m_dynLights.size(); i < ni; ++i)
	{
		SAIDynLightSource& light = m_dynLights[i];
		ColorB c = lightCols[(int)light.level];
		dc->DrawSphere(light.pos, 0.2f, c);
	}

	for (unsigned i = 0, ni = m_lights.size(); i < ni; ++i)
	{
		SAILightSource& light = m_lights[i];
		ColorB c = lightCols[(int)light.level];
		ColorB ct(c);
		ct.a = 64;
		dc->DrawSphere(light.pos, light.radius, ct);
		dc->DrawWireSphere(light.pos, light.radius, c);

		dc->Draw3dLabel(light.pos, 1.1f, "%s", g_szLightLevels[(int)light.level]);
	}

	for (unsigned i = 0, ni = m_dynLights.size(); i < ni; ++i)
	{
		SAIDynLightSource& light = m_dynLights[i];
		ColorB c = lightCols[(int)light.level];
		ColorB ct(c);
		float a = 1.0f - sqr(1.0f - light.t / (float)light.tmax);
		ct.a = (uint8)(32 + 32 * a);

		if (light.fov < 0)
		{
			// Omni
			dc->DrawSphere(light.pos, light.radius, ct);
			dc->DrawWireSphere(light.pos, light.radius, c);
		}
		else
		{
			// Spot
			float coneRadius = sinf(light.fov) * light.radius;
			float coneHeight = cosf(light.fov) * light.radius;
			Vec3 conePos = light.pos + light.dir * coneHeight;
			dc->DrawLine(light.pos, c, light.pos + light.dir * light.radius, c);
			dc->DrawCone(conePos, -light.dir, coneRadius, coneHeight, ct);
			dc->DrawWireFOVCone(light.pos, light.dir, light.radius, light.fov * 2.0f, c);
		}

		dc->Draw3dLabel(light.pos, 1.1f, "DYN %s\n%s", g_szLightLevels[(int)light.level], g_szLightType[(int)light.type]);

		if (light.refAttrib)
		{
			Vec3 vPos = light.refAttrib->GetPos();
			dc->DrawSphere(vPos, 0.15f, c);
			dc->Draw3dLabel(vPos, 1.1f, "Attrib");
		}
	}

	CDebugDrawContext dc1;
	dc1->SetBackFaceCulling(false);

	// AIShapes
	for (ShapeMap::const_iterator di = GetAISystem()->GetGenericShapes().begin(), dend = GetAISystem()->GetGenericShapes().end(); di != dend; ++di)
	{
		const SShape& sa = di->second;
		if (sa.lightLevel == AILL_NONE)
			continue;
		if (sa.height < 0.0001f)
			continue;
		DebugDrawArea(sa.shape, sa.aabb.min.z, sa.aabb.min.z + sa.height, lightCols[(int)sa.lightLevel]);
		Vec3 pos(sa.shape.front());
		pos.z = sa.aabb.min.z + sa.height / 2;
		dc1->Draw3dLabel(pos, 1.1f, "%s\n%s", di->first.c_str(), g_szLightLevels[(int)sa.lightLevel]);
	}
}

//===================================================================
// DebugDrawArea
//===================================================================
void CAILightManager::DebugDrawArea(const ListPositions& poly, float zmin, float zmax, ColorB color)
{
	CDebugDrawContext dc;

	static std::vector<Vec3> verts;
	unsigned npts = poly.size();
	verts.resize(npts);
	unsigned i = 0;
	for (ListPositions::const_iterator it = poly.begin(), end = poly.end(); it != end; ++it)
		verts[i++] = *it;

	AIAssert(npts > 0);

	for (i = 0; i < npts; ++i)
		verts[i].z = zmin;
	dc->DrawPolyline(&verts[0], npts, true, color, 1.0f);

	for (i = 0; i < npts; ++i)
		verts[i].z = zmax;
	dc->DrawPolyline(&verts[0], npts, true, color, 1.0f);

	for (i = 0; i < npts; ++i)
	{
		dc->DrawLine(verts[i], color,
		             Vec3(verts[i].x, verts[i].y, zmin), color, 1.0f);
	}

	ColorB colorTrans(color);
	colorTrans.a = 64;

	for (i = 0; i < npts; ++i)
	{
		unsigned j = (i + 1) % npts;
		Vec3 vi(verts[i].x, verts[i].y, zmin);
		Vec3 vj(verts[j].x, verts[j].y, zmin);
		dc->DrawTriangle(verts[i], colorTrans, vi, colorTrans, vj, colorTrans);
		dc->DrawTriangle(verts[i], colorTrans, vj, colorTrans, verts[j], colorTrans);
	}
}

//===================================================================
// GetLightLevelAt
//===================================================================
EAILightLevel CAILightManager::GetLightLevelAt(const Vec3& pos, const CAIActor* pAgent, bool* outUsingCombatLight)
{
	CCCPOINT(CAILightManager_GetLightLevelAt);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// Find ambient light level.

	// Start from time-of-day.
	EAILightLevel lightLevel = AILL_NONE;

	// Override from AIShapes
	for (ShapeMap::const_iterator di = GetAISystem()->GetGenericShapes().begin(), dend = GetAISystem()->GetGenericShapes().end(); di != dend; ++di)
	{
		const SShape& sa = di->second;

		if (sa.lightLevel <= lightLevel)
			continue;

		if (sa.height < 0.0001f)
			continue;

		if (sa.IsPointInsideShape(pos, true))
			lightLevel = sa.lightLevel;
	}

	// Based on the ambient light level.
	for (unsigned i = 0, ni = m_lights.size(); i < ni; ++i)
	{
		SAILightSource& light = m_lights[i];

		if (light.level <= lightLevel)
			continue;

		Vec3 diff = pos - light.pos;
		if (diff.GetLengthSquared() > sqr(light.radius))
			continue;

		lightLevel = light.level;
	}

	if (outUsingCombatLight)
		*outUsingCombatLight = false;

	for (unsigned i = 0, ni = m_dynLights.size(); i < ni; ++i)
	{
		SAIDynLightSource& light = m_dynLights[i];

		if (outUsingCombatLight)
		{
			if (light.refShooter == pAgent && (light.type == AILE_FLASH_LIGHT || light.type == AILE_LASER))
				*outUsingCombatLight = true;
		}

		if (light.level <= lightLevel)
			continue;

		Vec3 diff = pos - light.pos;
		if (diff.GetLengthSquared() > sqr(light.radius))
			continue;

		if (light.fov > 0)
		{
			diff.Normalize();
			float dot = light.dir.Dot(diff);
			const float thr = cosf(light.fov);

			if (dot < thr)
				continue;
		}

		lightLevel = light.level;
	}

	return (lightLevel == AILL_NONE) ? m_ambientLight : lightLevel;
}

//===================================================================
// UpdateLights
//===================================================================
void CAILightManager::UpdateLights()
{
	CCCPOINT(CAILightManager_UpdateLights);

	m_lights.clear();

	const short LIGHTSPOT_LIGHT = 401;
	const short LIGHTSPOT_MEDIUM = 402;
	const short LIGHTSPOT_DARK = 403;
	const short LIGHTSPOT_SUPERDARK = 404;

	short types[] = { LIGHTSPOT_LIGHT, LIGHTSPOT_MEDIUM, LIGHTSPOT_DARK, LIGHTSPOT_SUPERDARK };
	EAILightLevel levels[] = { AILL_LIGHT, AILL_MEDIUM, AILL_DARK, AILL_SUPERDARK };

	// Get light anchors
	for (unsigned int i = 0, n = sizeof types / sizeof types[0]; i < n; ++i)
	{
		const short type = types[i];
		for (AIObjectOwners::iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(type), end = gAIEnv.pAIObjectManager->m_Objects.end(); ai != end && ai->first == type; ++ai)
		{
			// (MATT) Strong so always valid {2009/03/25}
			CAIObject* pObj = ai->second.GetAIObject();
			if (!pObj->IsEnabled()) continue;
			m_lights.push_back(SAILightSource(pObj->GetPos(), pObj->GetRadius(), levels[i]));
		}
	}

	// Associate lights to areas
	/*	for (unsigned i = 0, ni = m_lights.size(); i < ni; ++i)
	   {
	    // Override from Navigation modifiers
	    for (SpecialAreaMap::const_iterator di = GetAISystem()->GetSpecialAreas().begin(), dend = GetAISystem()->GetSpecialAreas().end(); di != dend; ++di)
	    {
	      const SpecialArea& sa = di->second;
	      if (sa.lightLevel == AILL_NONE)
	        continue;
	      if (sa.fHeight < 0.0001f)
	        continue;

	      AABB aabb = sa.GetAABB();
	      aabb.min.z = sa.fMinZ;
	      aabb.max.z = sa.fMaxZ;
	      if (Overlap::Point_Polygon2D(m_lights[i].pos, sa.GetPolygon(), &aabb))
	        m_lights[i].pSa = &sa;
	    }

	    // Override from AIShapes
	    for (ShapeMap::const_iterator di = GetAISystem()->GetGenericShapes().begin(), dend = GetAISystem()->GetGenericShapes().end(); di != dend; ++di)
	    {
	      const SShape& sa = di->second;
	      if (sa.lightLevel == AILL_NONE)
	        continue;
	      if (sa.height < 0.0001f)
	        continue;
	      if (sa.IsPointInsideShape(pos, true))
	      {
	        lightLevel = sa.lightLevel;
	      }
	    }
	   }*/

	/*	const PodArray<ILightSource*>* pLightEnts = gEnv->p3DEngine->GetLightEntities();

	   for (unsigned i = 0, ni = pLightEnts->size(); i < ni; ++i)
	   {
	    ILightSource* pLightSource = *pLightEnts->Get(i);
	    SRenderLight& light = pLightSource->GetLightProperties();
	    if ((light.m_Flags & DLF_FAKE) || (light.m_Flags & DLF_DIRECTIONAL))
	      continue;

	    float illum = light.m_Color.r * 0.3f + light.m_Color.r * 0.59f + light.m_Color.r * 0.11f + light.m_SpecMult * 0.1f;

	    EAILightLevel level;
	    if (illum > 0.4f)
	      level = AILL_LIGHT;
	    else if (illum > 0.1f)
	      level = AILL_MEDIUM;
	    else
	      level = AILL_DARK;

	    if ((light.m_Flags & DLF_PROJECT) && light.m_fLightFrustumAngle < 90.0f)
	    {
	      // Spot
	      Vec3 dir = light.m_ObjMatrix.TransformVector(Vec3(1,0,0));
	      float fov = DEG2RAD(light.m_fLightFrustumAngle) * 0.7f;
	      float radius = light.m_fRadius * 0.9f;
	      m_lights.push_back(SAILightSource(light.m_Origin, dir.GetNormalized(),
	        radius, fov, level, illum));
	    }
	    else
	    {
	      // Omni
	      float radius = light.m_fRadius * 0.9f;
	      m_lights.push_back(SAILightSource(light.m_Origin, radius, level, illum));
	    }
	   }*/
}

//===================================================================
// UpdateTimeOfDay
//===================================================================
void CAILightManager::UpdateTimeOfDay()
{
	ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
	if (!pTOD)
	{
		m_ambientLight = AILL_LIGHT;
		return;
	}

	float hour = pTOD->GetTime();

	if (hour < 5.8f)
	{
		m_ambientLight = AILL_DARK;
	}
	else if (hour < 6.15f)
	{
		m_ambientLight = AILL_MEDIUM;
	}
	else if (hour < 18.0f)
	{
		m_ambientLight = AILL_LIGHT;
	}
	else if (hour < 19.5f)
	{
		m_ambientLight = AILL_MEDIUM;
	}
	else
	{
		m_ambientLight = AILL_DARK;
	}
}

// save/load
//===================================================================
void CAILightManager::SAIDynLightSource::Serialize(TSerialize ser)
{
	ser.Value("pos", pos);
	ser.Value("dir", dir);
	ser.Value("radius", radius);
	ser.Value("fov", fov);
	ser.EnumValue("level", level, AILL_NONE, AILL_LAST);

	refShooter.Serialize(ser, "refShooter");
	refShooter.Serialize(ser, "refAttrib");

	ser.EnumValue("type", type, AILE_GENERIC, AILE_LAST);
	ser.Value("t", t);
	ser.Value("tmax", tmax);
}

// save/load
//===================================================================
void CAILightManager::Serialize(TSerialize ser)
{
	ser.BeginGroup("AILightManager");
	int dynLightCount(m_dynLights.size());
	ser.Value("DinLightSize", dynLightCount);
	if (ser.IsReading())
	{
		m_dynLights.resize(dynLightCount);
	}
	char AILightGrpName[256];
	for (unsigned i = 0; i < m_dynLights.size(); ++i)
	{
		cry_sprintf(AILightGrpName, "AIDynLightSource-%u", i);
		ser.BeginGroup(AILightGrpName);
		{
			m_dynLights[i].Serialize(ser);
		}
		ser.EndGroup();
	}

	ser.Value("m_updated", m_updated);
	ser.EnumValue("m_ambientLight", m_ambientLight, AILL_NONE, AILL_LAST);
	ser.Value("m_lastUpdateTime", m_lastUpdateTime);
	ser.EndGroup();
}
