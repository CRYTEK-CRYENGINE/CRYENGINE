// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Fire Mode Plugin Parameters

-------------------------------------------------------------------------
History:
- 09:02:2011 : Created by Claire Allan

*************************************************************************/

#include "StdAfx.h"
#include "FireModePluginParams.h"
#include "GameXmlParamReader.h"
#include "FireModePlugin.h"
#include "ProjectileAutoAimHelper.h"


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_OPERATORS_WITH_ARRAYS(EFFECT_PARAMS_MEMBERS, EFFECT_PARAMS_ARRAYS, SEffectParams)

//------------------------------------------------------------------------
const CGameTypeInfo* SFireModePlugin_HeatingParams::GetPluginType() {
	return GetType();
}

//------------------------------------------------------------------------
const CGameTypeInfo* SFireModePlugin_HeatingParams::GetType()
{
	return CFireModePlugin_Overheat::GetStaticType();
}

//------------------------------------------------------------------------
void SFireModePlugin_HeatingParams::Reset(const XmlNodeRef& paramsNode, const XmlNodeRef& actionsNode, bool defaultInit)
{
	if (defaultInit)
	{
		attack = 0.0f;
		duration = 5.0f;
		decay = 0.333f;
		refire_heat = 1.f;

		helper[0] = "";
		helper[1] = "";
		effect[0] = "";
		effect[1] = "";

		overheating = "overheating";
		cooldown = "cooldown";
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("attack", attack);
		reader.ReadParamValue<float>("duration", duration);
		reader.ReadParamValue<float>("decay", decay);
		reader.ReadParamValue<float>("refire_heat", refire_heat);

		helper[0] = reader.ReadParamValue("helper_fp", helper[0].c_str());
		helper[1] = reader.ReadParamValue("helper_fp", helper[1].c_str());
		effect[0] = reader.ReadParamValue("effect_fp", effect[0].c_str());
		effect[1] = reader.ReadParamValue("effect_tp", effect[1].c_str());
	}

	if(actionsNode)
	{
		CGameXmlParamReader reader(actionsNode);

		overheating = reader.ReadParamValue("overheating", overheating.c_str());
		cooldown = reader.ReadParamValue("cooldown", cooldown.c_str());
	}
}

//------------------------------------------------------------------------
void SFireModePlugin_HeatingParams::PreLoadAssets()
{
	for (int i = 0; i < 2; i++)
	{
		if(!gEnv->pParticleManager->FindEffect(effect[i].c_str()))
		{
			effect[i].clear();
		}
	}
}

//------------------------------------------------------------------------
void SFireModePlugin_HeatingParams::GetMemoryUsage(ICrySizer * s) const
{
	for (int i=0; i<2; i++)
	{
		s->AddObject(helper[i]);
		s->AddObject(effect[i]);
	}
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------
const CGameTypeInfo* SFireModePlugin_RejectParams::GetPluginType()
{
	return GetType();
}

//------------------------------------------------------------------------
const CGameTypeInfo* SFireModePlugin_RejectParams::GetType()
{
	return CFireModePlugin_Reject::GetStaticType();
}

//------------------------------------------------------------------------
void SFireModePlugin_RejectParams::Reset(const XmlNodeRef& paramsNode, const XmlNodeRef& actionsNode, bool defaultInit)
{
	rejectEffect.Reset(paramsNode, defaultInit);
}

//------------------------------------------------------------------------
void SFireModePlugin_RejectParams::PreLoadAssets()
{
	rejectEffect.PreLoadAssets();
}

//------------------------------------------------------------------------
void SFireModePlugin_RejectParams::GetMemoryUsage(ICrySizer * s) const
{
	rejectEffect.GetMemoryUsage(s);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SFireModePlugin_RecoilShakeParams::SRecoilShakeParams::SetDefaults()
{
	m_rotateShake.Set(0.0f, 0.0f, 0.0f);
	m_shiftShake.Set(0.0f, 0.0f, 0.0f);
	m_frequency = 1.0f;
	m_shakeTime = 0.1f;
	m_randomness = 0.0f;
}

void SFireModePlugin_RecoilShakeParams::SRecoilShakeParams::ReadParams(const XmlNodeRef& paramsNode)
{
	assert(paramsNode != NULL);

	CGameXmlParamReader reader(paramsNode);

	Vec3 shakeInDegrees(m_rotateShake);
	reader.ReadParamValue("rotateShake", shakeInDegrees);
	m_rotateShake.Set( DEG2RAD(shakeInDegrees.x), DEG2RAD(shakeInDegrees.y), DEG2RAD(shakeInDegrees.z) );

	reader.ReadParamValue("shiftShake", m_shiftShake);
	reader.ReadParamValue("frequency", m_frequency);
	reader.ReadParamValue("time", m_shakeTime);
	reader.ReadParamValue("randomness", m_randomness);
}

SFireModePlugin_RecoilShakeParams::~SFireModePlugin_RecoilShakeParams()
{

}

const CGameTypeInfo* SFireModePlugin_RecoilShakeParams::GetPluginType()
{
	return GetType();
}

const CGameTypeInfo* SFireModePlugin_RecoilShakeParams::GetType()
{
	return CFireModePlugin_RecoilShake::GetStaticType();
}

void SFireModePlugin_RecoilShakeParams::Reset(const XmlNodeRef& paramsNode, const XmlNodeRef& actionsNode, bool defaultInit)
{
	if(defaultInit)
	{
		m_recoilShake.SetDefaults();
	}

	if(paramsNode != NULL)
	{
		m_recoilShake.ReadParams(paramsNode);
	}
}

void SFireModePlugin_RecoilShakeParams::PreLoadAssets()
{

}

void SFireModePlugin_RecoilShakeParams::GetMemoryUsage(ICrySizer * s) const
{

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------
void SEffectParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit/*=true*/ )
{
	if (defaultInit)
	{
		effect[0].clear(); effect[1].clear();
		helper[0].clear(); helper[1].clear();
		scale[0] = scale[1] = 1.0f;
		time[0] = time[1] = 0.060f;
		aiVisibilityRadius = 0.0f;
		helperFromAccessory = false;
	}

	if (paramsNode)
	{
		int _helperFromAccessory = 0;
		paramsNode->getAttr("aiVisibilityRadius", aiVisibilityRadius);
		paramsNode->getAttr("helperFromAccessory", _helperFromAccessory);

		helperFromAccessory = _helperFromAccessory != 0;

		CGameXmlParamReader reader(paramsNode);

		XmlNodeRef fpNode = reader.FindFilteredChild("firstperson");
		if (fpNode)
		{ 
			effect[0] = fpNode->getAttr("effect");
			helper[0] = fpNode->getAttr("helper");					
			fpNode->getAttr("scale", scale[0]);
			fpNode->getAttr("time", time[0]);
		}

		XmlNodeRef tpNode = reader.FindFilteredChild("thirdperson");
		if (tpNode)
		{ 
			effect[1] = tpNode->getAttr("effect");
			helper[1] = tpNode->getAttr("helper");					
			tpNode->getAttr("scale", scale[1]);
			tpNode->getAttr("time", time[1]);
		}

	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const CGameTypeInfo* SFireModePlugin_AutoAimParams::GetPluginType()
{
	return GetType();
}

const CGameTypeInfo* SFireModePlugin_AutoAimParams::GetType()
{
	return CFireModePlugin_AutoAim::GetStaticType();
}

void SFireModePlugin_AutoAimParams::Reset( const XmlNodeRef& paramsNode, const XmlNodeRef& actionsNode, bool defaultInit /*= true*/ )
{
	if (defaultInit)
	{
		m_normalParams.SetDefaults();
		m_zoomedParams.SetDefaults(); 
	}

	if (paramsNode)
	{
		const XmlNodeRef& normalNode = paramsNode->findChild("normal");
		if(normalNode)
		{
			m_normalParams.ReadParams(normalNode);
		}
		const XmlNodeRef& zoomedNode = paramsNode->findChild("zoomed");
		if(zoomedNode)
		{
			m_zoomedParams.ReadParams(zoomedNode);
		}
	}
}

void SFireModePlugin_AutoAimParams::SAutoAimModeParams::SetDefaults()
{
	m_enabled = false;
	m_maxDistance	= 100.0f;
	m_minDistance	= 5.0f;
	m_outerConeRads	= DEG2RAD(20.0f);
	m_innerConeRads	= DEG2RAD(5.0f);
}

void SFireModePlugin_AutoAimParams::SAutoAimModeParams::ReadParams( const XmlNodeRef& paramsNode )
{
	CGameXmlParamReader reader(paramsNode);

	reader.ReadParamValue<float>("maxDistance",	m_maxDistance);
	reader.ReadParamValue<float>("minDistance",	m_minDistance);
	if(reader.ReadParamValue<float>("outerConeDegs",	m_outerConeRads))
		m_outerConeRads = DEG2RAD(m_outerConeRads);
	if(reader.ReadParamValue<float>("innerConeDegs",	m_innerConeRads))
		m_innerConeRads = DEG2RAD(m_innerConeRads);

	m_enabled = (m_outerConeRads>=m_innerConeRads && m_maxDistance>m_minDistance);
}
