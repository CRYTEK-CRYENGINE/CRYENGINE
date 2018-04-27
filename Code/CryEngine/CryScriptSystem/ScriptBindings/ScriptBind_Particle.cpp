// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ScriptBind_Particle.cpp
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ScriptBind_Particle.h"
#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryParticleSystem/ParticleParams.h>

// TypeInfo implementations
#ifndef _LIB
	#include <CryParticleSystem/ParticleParams_TypeInfo.h>
	#include <CryCore/Common_TypeInfo.h>
#else
	#include <CryCore/CryTypeInfo.h>
#endif

//------------------------------------------------------------------------
CScriptBind_Particle::CScriptBind_Particle(IScriptSystem* pScriptSystem, ISystem* pSystem)
{
	CScriptableBase::Init(pScriptSystem, pSystem);
	SetGlobalName("Particle");

	m_pSystem = pSystem;
	m_p3DEngine = gEnv->p3DEngine;

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Particle::

	SCRIPT_REG_TEMPLFUNC(SpawnEffect, "effectName, pos, dir, [scale], [entityId], [partId]");
	SCRIPT_REG_TEMPLFUNC(EmitParticle, "pos, dir, entityId, slotId");
	SCRIPT_REG_TEMPLFUNC(CreateDecal, "pos, normal, size, lifeTime, textureName, [angle], [hitDirection], [entityId], [partid]");
	SCRIPT_REG_TEMPLFUNC(CreateMatDecal, "pos, normal, size, lifeTime, materialName, [angle], [hitDirection], [entityId], [partid]");
}

CScriptBind_Particle::~CScriptBind_Particle()
{
}

//------------------------------------------------------------------------
int CScriptBind_Particle::SpawnEffect(IFunctionHandler* pH, const char* effectName, Vec3 pos, Vec3 dir)
{
	IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(effectName, "Particle.SpawnEffect");

	int entitySlot = -1;

	if (pEffect)
	{
		float scale = 1.0f;
		if ((pH->GetParamCount() > 3) && (pH->GetParamType(4) == svtNumber))
			pH->GetParam(4, scale);

		ScriptHandle entityId(0);
		if ((pH->GetParamCount() > 4) && (pH->GetParamType(5) == svtPointer))
			pH->GetParam(5, entityId);

		int partId = 0;
		if ((pH->GetParamCount() > 5) && (pH->GetParamType(6) == svtNumber))
			pH->GetParam(6, partId);

		IEntity* pEntity = NULL;
		if (entityId.n)
			pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n);
		if (pEntity)
			entitySlot = pEntity->LoadParticleEmitter(-1, pEffect);
		else
			pEffect->Spawn(true, IParticleEffect::ParticleLoc(pos, dir, scale));
	}

	return pH->EndFunction(entitySlot);
}

//------------------------------------------------------------------------
int CScriptBind_Particle::EmitParticle(IFunctionHandler* pH, Vec3 pos, Vec3 dir, ScriptHandle entityId, int slotId)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n);
	if (!pEntity)
		return pH->EndFunction();

	IParticleEmitter* pEmitter = pEntity->GetParticleEmitter(slotId);
	if (!pEmitter)
		return pH->EndFunction();

	EmitParticleData emitData;
	emitData.bHasLocation = true;
	emitData.Location = IParticleEffect::ParticleLoc(pos, dir, 1.0f);
	pEmitter->EmitParticle(&emitData);

	return pH->EndFunction();
}

//------------------------------------------------------------------------

void CScriptBind_Particle::CreateDecalInternal(IFunctionHandler* pH, const Vec3& pos, const Vec3& normal, float size, float lifeTime, const char* name, bool nameIsMaterial)
{
	CryEngineDecalInfo decal;

	decal.vPos = pos;
	decal.vNormal = normal;
	decal.fSize = size;
	decal.fLifeTime = lifeTime;

	if (nameIsMaterial)
	{
		cry_strcpy(decal.szMaterialName, name);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "CScriptBind_Particle::CreateDecalInternal: Decal material name is not specified");
	}

	if ((pH->GetParamCount() > 5) && (pH->GetParamType(6) != svtNull))
	{
		pH->GetParam(6, decal.fAngle);
	}

	if ((pH->GetParamCount() > 6) && (pH->GetParamType(7) != svtNull))
	{
		pH->GetParam(7, decal.vHitDirection);
	}
	else
	{
		decal.vHitDirection = -normal;
	}

	if ((pH->GetParamCount() > 7) && (pH->GetParamType(8) != svtNull))
	{
		ScriptHandle entityId;

		pH->GetParam(8, entityId);

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n);

		if (pEntity)
		{
			IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
			;

			
			{
				decal.ownerInfo.pRenderNode = pIEntityRender->GetRenderNode();
			}
		}
	}
	else if ((pH->GetParamCount() > 8) && (pH->GetParamType(9) != svtNull))
	{
		ScriptHandle renderNode;
		pH->GetParam(9, renderNode);
		decal.ownerInfo.pRenderNode = (IRenderNode*)renderNode.ptr;
	}

	if ((pH->GetParamCount() > 9) && (pH->GetParamType(10) != svtNull))
	{
		float growTime;

		pH->GetParam(10, growTime);
		decal.fGrowTime = growTime;
	}

	if ((pH->GetParamCount() > 10) && (pH->GetParamType(11) != svtNull))
	{
		float growTimeAlpha = 0.0f;

		pH->GetParam(11, growTimeAlpha);
		decal.fGrowTimeAlpha = growTimeAlpha;
	}

	// [*DavidR | 27/Sep/2010] ToDo: 12 parameters? this bind needs to work with a table as parameter instead of this
	if ((pH->GetParamCount() > 11) && (pH->GetParamType(12) != svtNull))
	{
		bool skip = false;

		pH->GetParam(12, skip);
		decal.bSkipOverlappingTest = skip;
	}

	m_p3DEngine->CreateDecal(decal);
}

int CScriptBind_Particle::CreateDecal(IFunctionHandler* pH, Vec3 pos, Vec3 normal, float size, float lifeTime, const char* textureName)
{
	CreateDecalInternal(pH, pos, normal, size, lifeTime, textureName, false);
	return pH->EndFunction();
}

int CScriptBind_Particle::CreateMatDecal(IFunctionHandler* pH, Vec3 pos, Vec3 normal, float size, float lifeTime, const char* materialName)
{
	CreateDecalInternal(pH, pos, normal, size, lifeTime, materialName, true);
	return pH->EndFunction();
}

template<class T>
bool TryReadType(const CTypeInfo::CVarInfo& var, T* data, SmartScriptTable& table, cstr key)
{
	T val;
	return table.GetPtr()->GetValue(key, val)
	       && var.Type.FromValue(data, &val, TypeInfo(&val));
}

void CScriptBind_Particle::ReadParams(SmartScriptTable& table, ParticleParams* pParams, IParticleEffect* pEffect)
{
	// Iterate params using TypeInfo, read from script table.
	const CTypeInfo& paramsType = TypeInfo(pParams);

	IScriptTable::Iterator it = table.GetPtr()->BeginIteration();
	while (table.GetPtr()->MoveNext(it))
	{
		const CTypeInfo::CVarInfo* pVar = paramsType.FindSubVar(it.sKey);
		if (!pVar)
		{
			m_pSS->RaiseError("<Particle::ReadParams> Invalid parameter %s", it.sKey);
			continue;
		}

		// Strange LUA stuff here.
		void* data = pVar->GetAddress(pParams);

		if (&pVar->Type == &TypeInfo((string*)data))
		{
			cstr val;
			if (table.GetPtr()->GetValue(it.sKey, val))
			{
				*(string*)data = val;
				continue;
			}
		}
		else if (TryReadType(*pVar, (bool*)data, table, it.sKey) ||
		         TryReadType(*pVar, (int*)data, table, it.sKey) ||
		         TryReadType(*pVar, (float*)data, table, it.sKey))
			continue;

		m_pSS->RaiseError("<Particle::ReadParams> Cannot read parameter %s of type %s", it.sKey, pVar->Type.Name);
	}
	table.GetPtr()->EndIteration(it);

	if (pEffect)
	{
		pEffect->SetParticleParams(*pParams);
	}
}
